#include "PcmPipeCases.hpp"
#include "audio/AudioFrame.hpp"
#include <limits>

int main()
{
    try
    {
        pipeDiscardAlignmentTest();
        tonal::audio::AudioFrame frame;
        frame.samples = {.125f, -.25f};
        checkPcm(frame.validPcm(), "valid stereo PCM");
        for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(), 1.e20f})
        {
            frame.samples[1] = bad;
            checkPcm(!frame.validPcm(), "invalid PCM accepted");
        }
        frame.samples = {2.f, -2.f};
        checkPcm(frame.validPcm(), "legitimate float headroom rejected");
        frame.samples.push_back(0);
        checkPcm(!frame.validPcm(), "incomplete stereo frame accepted");
        frame.samples.clear();
        frame.format.channels.clear();
        checkPcm(!frame.validPcm(), "missing channel layout accepted");
        std::cout << "PASS PCM validity/headroom guard\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
