/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/fswatcher.h
// Purpose:     wxPollingFileSystemWatcher
// Author:      Bartosz Bekier
// Created:     2009-05-26
// RCS-ID:      $Id: fswatcher.h 62474 2009-10-22 11:35:43Z VZ $
// Copyright:   (c) 2009 Bartosz Bekier <bartosz.bekier@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FSWATCHER_GENERIC_H_
#define _WX_FSWATCHER_GENERIC_H_

#include "wx/defs.h"

#if wxUSE_FSWATCHER

class WXDLLIMPEXP_BASE wxPollingFileSystemWatcher : public wxFileSystemWatcherBase
{
public:

};

#endif // wxUSE_FSWATCHER

#endif /* _WX_FSWATCHER_GENERIC_H_ */
