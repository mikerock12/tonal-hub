"""Reproducible ADB measurement. Never reads account files or credentials."""
import argparse
import json
from pathlib import Path
import re
import statistics
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('--adb', required=True)
parser.add_argument('--package', required=True)
parser.add_argument('--scenario', choices=['idle', 'local', 'effects', 'streaming'], default='idle')
parser.add_argument('--label', required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--samples', type=int, default=8)
parser.add_argument('--interval', type=float, default=5)
parser.add_argument('--warmup', type=float, default=15)
parser.add_argument('--no-restart', action='store_true')
parser.add_argument('--cold-starts', type=int, default=0)
parser.add_argument('--background', action='store_true', help='Press Home before warmup; process remains cached')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

def adb(*command, check=True):
    result = subprocess.run([args.adb, *command], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=45)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result.stdout.strip()

def ticks():
    result = {}
    for line in adb('shell', 'ps', '-A', '-o', 'PID,NAME').splitlines()[1:]:
        fields = line.split()
        if len(fields) != 2 or not fields[0].isdigit():
            continue
        pid, name = fields
        if name != args.package and not name.startswith(args.package + ':'):
            continue
        stat = adb('shell', 'cat', f'/proc/{pid}/stat', check=False)
        if ')' not in stat:
            continue
        fields = stat.rsplit(')', 1)[1].split()
        result[pid] = int(fields[11]) + int(fields[12])
    return result

component = adb('shell', 'cmd', 'package', 'resolve-activity', '--brief', '-a', 'android.intent.action.MAIN', '-c', 'android.intent.category.LAUNCHER', args.package).splitlines()[-1]
if '/' not in component:
    raise RuntimeError('Launcher activity not found')

def launch():
    command = ['shell', 'am', 'start', '-S', '-W', '-n', component]
    if args.scenario in ('local', 'effects'):
        command += ['--ez', 'benchmark', 'true']
        if args.scenario == 'effects':
            command += ['--ez', 'effects', 'true']
    return adb(*command)

profile = {
    'android': adb('shell', 'getprop', 'ro.build.version.release'),
    'api': adb('shell', 'getprop', 'ro.build.version.sdk'),
    'abi': adb('shell', 'getprop', 'ro.product.cpu.abi'),
    'cores': int(adb('shell', 'getconf', '_NPROCESSORS_ONLN')),
    'page_size': int(adb('shell', 'getconf', 'PAGESIZE')),
    'clk_tck': int(adb('shell', 'getconf', 'CLK_TCK')),
}
package_dump = adb('shell', 'dumpsys', 'package', args.package)
version = re.search(r'versionName=([^\s]+)', package_dump)
profile['app_version'] = version.group(1) if version else None
cold = []
for trial in range(args.cold_starts):
    startup = launch()
    (args.output / f'startup-{trial+1}.txt').write_text(startup, encoding='utf-8')
    match = re.search(r'TotalTime:\s*(\d+)', startup)
    if match:
        cold.append(int(match.group(1)))
    time.sleep(2)
if not args.no_restart:
    launch()
if args.background:
    adb('shell', 'input', 'keyevent', '3')
profile['visibility'] = 'background' if args.background else 'foreground'
(args.output / 'activity-before.txt').write_text(adb('shell', 'dumpsys', 'activity', 'activities'), encoding='utf-8')
print(f'{args.label}: warming up {args.warmup:g}s', flush=True)
time.sleep(args.warmup)
rows = []
for index in range(args.samples):
    before = ticks()
    start = time.monotonic()
    time.sleep(args.interval)
    after = ticks()
    elapsed = time.monotonic() - start
    if not after:
        raise RuntimeError('Application not running or CPU counters unavailable')
    cpu_ticks = sum(max(0, after[pid] - before[pid]) for pid in before.keys() & after.keys())
    # Cached Android processes can be frozen. Do not make a Binder request into
    # the app: it can time out or wake it and perturb the next CPU sample.
    memory = adb('shell', 'dumpsys', 'meminfo', '--local', '-s', '-S', args.package)
    (args.output / f'meminfo-{index+1}.txt').write_text(memory, encoding='utf-8')
    pss = sum(map(int, re.findall(r'TOTAL PSS:\s*(\d+)', memory)))
    if not pss:
        raise RuntimeError('PSS summary not found')
    swap_values = re.findall(r'TOTAL SWAP PSS:\s*(\d+)', memory)
    swap_kind = 'SwapPSS' if swap_values else 'SWAP_KB'
    swap_values = swap_values or re.findall(r'TOTAL SWAP \(KB\):\s*(\d+)', memory)
    swap_kib = sum(map(int, swap_values))
    cpu = cpu_ticks / profile['clk_tck'] / elapsed * 100
    row = dict(sample=index+1, pss_kib=pss, swap_kib=swap_kib, swap_kind=swap_kind, pss_plus_swap_kib=pss+swap_kib, cpu_ticks=cpu_ticks, counters_before=before, counters_after=after, cpu_one_core_percent=cpu,
               cpu_device_percent=cpu/profile['cores'], processes=len(after), interval_seconds=elapsed)
    rows.append(row)
    print(f"{args.label} {index+1}/{args.samples}: PSS {pss/1024:.1f} MiB; CPU {cpu:.2f}% of one core; processes {len(after)}", flush=True)
result = dict(label=args.label, package=args.package, scenario=args.scenario, profile=profile,
              cold_start_ms=cold, samples=rows,
              median_pss_mib=statistics.median(r['pss_kib'] for r in rows)/1024,
              median_cpu_one_core_percent=statistics.median(r['cpu_one_core_percent'] for r in rows),
              cpu_definition='100% = one fully used core; all named package processes summed',
              memory_definition='dumpsys meminfo --local -s -S; TOTAL PSS and SwapPSS for named package processes; shared pages apportioned')
(args.output / 'activity-after.txt').write_text(adb('shell', 'dumpsys', 'activity', 'activities'), encoding='utf-8')
(args.output / 'audio-after.txt').write_text(adb('shell', 'dumpsys', 'media.audio_flinger'), encoding='utf-8')
(args.output / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k.startswith('median_')}, indent=2), flush=True)
