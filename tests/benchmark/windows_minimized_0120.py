"""Matched minimized-window follow-up. Minimize using the approved GUI tool first."""
import json,time
import windows_0120 as b

def minimized(app):
    state=b.windows(app)
    assert state and all(w['minimized'] for w in state),f'{app} must remain minimized: {state}'

def main():
    assert not any('qemu' in (p.info['name'] or '').lower() for p in b.psutil.process_iter(['name']))
    devices=b.probe.control('devices')
    targets={'tonalify':next(d for d in devices if d['name'].startswith('Tonal @')),
             'spotify':next(d for d in devices if d['type']=='Computer' and not d['name'].startswith('Tonal'))}
    records=[];proofs=[]
    for round_number,order in [(1,['tonalify','spotify']),(2,['spotify','tonalify'])]:
        for app in order:
            minimized(app)
            b.start_track(app,targets);time.sleep(12)
            minimized(app);proofs.append(b.proof(app,targets[app],'before',round_number))
            records.append(b.measure(app,'playing-minimized',round_number))
            minimized(app);proofs.append(b.proof(app,targets[app],'after',round_number))
            b.probe.control('pause',DeviceId=targets[app]['id']);time.sleep(8)
            assert not b.probe.control('state')['playing']
            minimized(app);records.append(b.measure(app,'paused-minimized',round_number));minimized(app)
            (b.OUT/'minimized-progress.json').write_text(json.dumps(dict(results=records,proofs=proofs),indent=2),encoding='utf-8')
    (b.OUT/'minimized-summary.json').write_text(json.dumps(dict(results=records,proofs=proofs),indent=2),encoding='utf-8')
    print('MINIMIZED_0120_COMPLETE',flush=True)

if __name__=='__main__':main()
