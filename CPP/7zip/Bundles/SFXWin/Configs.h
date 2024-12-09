
#ifndef SFX_CONFIGS_H
#define SFX_CONFIGS_H

#include "Common/MyString.h"

#ifdef DEFINE_VARS
#define EXTERN
#else  // DEFINE_VARS
#define EXTERN extern
#endif  // DEFINE_VARS

struct Configs {
  UString szTitle;
  UString szErrorTitle;
  UString szExtractPathTitle;
  UString szExtractPathLabel;
};

EXTERN Configs g_Configs;



#endif  // SFX_CONFIGS_H
