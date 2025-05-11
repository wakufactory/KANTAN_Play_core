#include "common_define.hpp"

namespace kanplay_ns {
namespace def {
namespace play {
//-------------------------------------------------------------------------
const char* GetVoicingName(KANTANMusic_Voicing voicing) {
  switch (voicing) {
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Close:                  return "Close";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Guitar:                 return "Guitar";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Static:                 return "Static";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Ukulele:                return "Ukulele";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Melody_Major:           return "M-Major";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Melody_MajorPentatonic: return "M-Penta";
    case KANTANMusic_Voicing::KANTANMusic_Voicing_Melody_Chromatic:       return "M-Chroma";
    default: return "----";
  }
}

//-------------------------------------------------------------------------
}
}
} // namespace kanplay_ns

