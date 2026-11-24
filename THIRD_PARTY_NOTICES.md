# Third-party notices

## CLAP SDK

`clap-sdk/include/` holds the CLAP 1.2 headers from <https://github.com/free-audio/clap>,
under the MIT license in `clap-sdk/LICENSE`. Only the header directory is vendored.

## RPVerb data

`engine/rpverb_core.hpp` contains values copied from the RPVerb 1.0.7 Rack Extension with the
repository owner's approval: the Space (18 x 10) and Colour (5 x 12) preset tables, the EQ
rotation table and band normalisers, the comb tunings and the parameter defaults. The allpass
lengths and eight of the comb lengths are the published Freeverb tunings. No original
executable or resource file is included. RP-Verb is a product of Rob Papen; this project is
not affiliated with or endorsed by Rob Papen or Reason Studios.
