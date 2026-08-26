// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_CANVAS_CANVAS_NATIVE_H
#define RUBIDIUM_CANVAS_CANVAS_NATIVE_H

// Linux-specific native canvas implementation.
//
// This translation unit is compiled on Linux only. It is listed in the MSVC
// project (msvc/Rubidium.vcxproj) as a <None> item so it shows up in the IDE
// for navigation, but is excluded from the Windows build. CMake keeps it in
// the source tree on every host and marks it HEADER_FILE_ONLY off-Linux.

namespace RUBIDIUM
{
   class CANVAS_NATIVE : public CANVAS
   {
   public:
      CANVAS_NATIVE (LOGGER* pLogger);
      ~CANVAS_NATIVE ();

      bool Initialize (void* pParentHandle, int nWidth, int nHeight) override;
      void SetVisible (bool bVisible)                                override;

   protected:
      void ApplyChildGeometry () override;
      void RaiseChild ()         override;
   };
}

#endif // RUBIDIUM_CANVAS_CANVAS_NATIVE_H
