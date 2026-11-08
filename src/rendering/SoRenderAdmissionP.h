#ifndef COIN_SORENDERADMISSIONP_H
#define COIN_SORENDERADMISSIONP_H

#ifndef COIN_INTERNAL
#error this is a private header file
#endif

#include <Inventor/actions/SoIRRenderAction.h>

enum class SoRetainedFrameAdmission {
  ADMITTED,
  TRAVERSAL_UNSUPPORTED,
  APPLICATION_COVERAGE_INVALID
};

inline SoRetainedFrameAdmission
coin_evaluate_retained_frame_admission(const SoIRRenderAction & action)
{
  if (action.hasUnsupportedRendering()) {
    return SoRetainedFrameAdmission::TRAVERSAL_UNSUPPORTED;
  }
  if (!action.isApplicationCoverageReceiptCurrent()) {
    return SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID;
  }
  return SoRetainedFrameAdmission::ADMITTED;
}

inline const char *
coin_retained_frame_admission_reason(const SoRetainedFrameAdmission verdict)
{
  switch (verdict) {
  case SoRetainedFrameAdmission::ADMITTED:
    return nullptr;
  case SoRetainedFrameAdmission::TRAVERSAL_UNSUPPORTED:
    return "unsupported retained rendering semantics";
  case SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID:
    return "application coverage receipt is incomplete or stale";
  }
  return "invalid retained frame admission verdict";
}

#endif // COIN_SORENDERADMISSIONP_H
