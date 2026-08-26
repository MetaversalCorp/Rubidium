// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_SHELL_APPNATIVE_H
#define RUBIDIUM_SHELL_APPNATIVE_H

namespace RUBIDIUM
{
   class APPNATIVE : public APP
   {
   private:
      class Impl;
      Impl* m_pImpl;

   public:
      static APPNATIVE* GetInstance ();

      int Run ();

   private:
      APPNATIVE ();
      ~APPNATIVE ();

      APPNATIVE (const APPNATIVE&)     = delete; // Prevent Clone
      void operator=(const APPNATIVE&) = delete; // Prevent Assignment
   };
}

#endif // RUBIDIUM_SHELL_APPNATIVE_H
