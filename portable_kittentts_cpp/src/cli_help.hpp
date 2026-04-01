#pragma once

#include <ostream>

inline void print_cli_help(std::ostream& os, bool include_bootstrapper_options = false)
{
    os
        << "Kitten TTS portable C++ CLI\n"
        << "Usage:\n"
        << "  kitten_tts --text \"Hello world\" [--speaker male] [--speed 1.0] [--output infer_outputs/output.wav]\n"
        << "  kitten_tts --list-speakers\n"
        << "  kitten_tts            # interactive mode\n\n"
        << "Options:\n"
        << "  --speaker NAME|INDEX   Bella, Jasper, Luna, Bruno, Rosie, Hugo, Kiki, Leo\n"
        << "  --speaker male|female  Compatibility aliases\n"
        << "  --speed FLOAT          Speech speed, default 1.0\n"
        << "  --device cpu           Accepted for compatibility; CPU only in this build\n"
        << "  --model nano|nano-int8|micro|mini Select the bundled model\n"
        << "  --session [NAME]       Interactive mode, or a named reusable session when NAME is provided\n"
        << "  --send                 Send one command to a named session and exit\n"
        << "  --terminate            Stop a named session and exit\n"
        << "  --clean-text           Enable number / currency expansion before phonemizing\n"
        << "  --output FILE.wav      WAV output path\n"
        << "  --list-speakers        Print the voice list\n";

    if (include_bootstrapper_options)
    {
        os
            << "\nBootstrapper-only options:\n"
            << "  --coldstart            Force a fresh temporary extraction instead of using the cache\n";
    }
}
