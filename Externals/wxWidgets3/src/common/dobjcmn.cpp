///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dobjcmn.cpp
// Purpose:     implementation of data object methods common to all platforms
// Author:      Vadim Zeitlin, Robert Roebling
// Modified by:
// Created:     19.10.99
// RCS-ID:      $Id: dobjcmn.cpp 70908 2012-03-15 13:49:49Z VZ $
// Copyright:   (c) wxWidgets Team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DATAOBJ

#include "wx/dataobj.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

// ----------------------------------------------------------------------------
// lists
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxSimpleDataObjectList)

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

static wxDataFormat dataFormatInvalid;
WXDLLEXPORT const wxDataFormat& wxFormatInvalid = dataFormatInvalid;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDataObjectBase
// ----------------------------------------------------------------------------

wxDataObjectBase::~wxDataObjectBase()
{
}

bool wxDataObjectBase::IsSupported(const wxDataFormat& format,
                                   Direction dir) const
{
    size_t nFormatCount = GetFormatCount( dir );
    if ( nFormatCount == 1 )
    {
        return format == GetPreferredFormat( dir );
    }
    else
    {
        wxDataFormat *formats = new wxDataFormat[nFormatCount];
        GetAllFormats( formats, dir );

        size_t n;
        for ( n = 0; n < nFormatCount; n++ )
        {
            if ( formats[n] == format )
                break;
        }

        delete [] formats;

        // found?
        return n < nFormatCount;
    }
}

// ----------------------------------------------------------------------------
// wxDataObjectComposite
// ----------------------------------------------------------------------------

wxDataObjectComposite::wxDataObjectComposite()
{
    m_preferred = 0;
    m_receivedFormat = wxFormatInvalid;
}

wxDataObjectComposite::~wxDataObjectComposite()
{
    WX_CLEAR_LIST( wxSimpleDataObjectList, m_dataObjects );
}

wxDataObjectSimple *
wxDataObjectComposite::GetObject(const wxDataFormat& format, wxDataObjectBase::Direction dir) const
{
    wxSimpleDataObjectList::compatibility_iterator node = m_dataObjects.GetFirst();

    while ( node )
    {
        wxDataObjectSimple *dataObj = node->GetData();

        if (dataObj->IsSupported(format,dir))
          return dataObj;
        node = node->GetNext();
    }
    return NULL;
}

void wxDataObjectComposite::Add(wxDataObjectSimple *dataObject, bool preferred)
{
    if ( preferred )
        m_preferred = m_dataObjects.GetCount();

    m_dataObjects.Append( dataObject );
}

wxDataFormat wxDataObjectComposite::GetReceivedFormat() const
{
    return m_receivedFormat;
}

wxDataFormat
wxDataObjectComposite::GetPreferredFormat(Direction WXUNUSED(dir)) const
{
    wxSimpleDataObjectList::compatibility_iterator node = m_dataObjects.Item( m_preferred );

    wxCHECK_MSG( node, wxFormatInvalid, wxT("no preferred format") );

    wxDataObjectSimple* dataObj = node->GetData();

    return dataObj->GetFormat();
}

#if defined(__WXMSW__)

size_t wxDataObjectComposite::GetBufferOffset( const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetBufferOffset( format );
}


const void* wxDataObjectComposite::GetSizeFromBuffer( const void* buffer,
                                                      size_t* size,
                                                      const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetSizeFromBuffer( buffer, size, format );
}


void* wxDataObjectComposite::SetSizeInBuffer( void* buffer, size_t size,
                                              const wxDataFormat& format )
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, NULL,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->SetSizeInBuffer( buffer, size, format );
}

#endif

size_t wxDataObjectComposite::GetFormatCount(Direction dir) const
{
    size_t n = 0;

    // NOTE: some wxDataObjectSimple objects may return a number greater than 1
    //       from GetFormatCount(): this is the case of e.g. wxTextDataObject
    //       under wxMac and wxGTK
    wxSimpleDataObjectList::compatibility_iterator node;
    for ( node = m_dataObjects.GetFirst(); node; node = node->GetNext() )
        n += node->GetData()->GetFormatCount(dir);

    return n;
}

void wxDataObjectComposite::GetAllFormats(wxDataFormat *formats,
                                          Direction dir) const
{
    size_t index(0);
    wxSimpleDataObjectList::compatibility_iterator node;

    for ( node = m_dataObjects.GetFirst(); node; node = node->GetNext() )
    {
        // NOTE: some wxDataObjectSimple objects may return more than 1 format
        //       from GetAllFormats(): this is the case of e.g. wxTextDataObject
        //       under wxMac and wxGTK
        node->GetData()->GetAllFormats(formats+index, dir);
        index += node->GetData()->GetFormatCount(dir);
    }
}

size_t wxDataObjectComposite::GetDataSize(const wxDataFormat& format) const
{
    wxDataObjectSimple *dataObj = GetObject(format);

    wxCHECK_MSG( dataObj, 0,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataSize();
}

bool wxDataObjectComposite::GetDataHere(const wxDataFormat& format,
                                        void *buf) const
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, false,
                 wxT("unsupported format in wxDataObjectComposite"));

    return dataObj->GetDataHere( buf );
}

bool wxDataObjectComposite::SetData(const wxDataFormat& format,
                                    size_t len,
                                    const void *buf)
{
    wxDataObjectSimple *dataObj = GetObject( format );

    wxCHECK_MSG( dataObj, false,
                 wxT("unsupported format in wxDataObjectComposite"));

    m_receivedFormat = format;

    // Notice that we must pass "format" here as wxTextDataObject, that we can
    // have as one of our "simple" sub-objects actually is not that simple and
    // can support multiple formats (ASCII/UTF-8/UTF-16/...) and so needs to
    // know which one it is given.
    return dataObj->SetData( format, len, buf );
}

// ----------------------------------------------------------------------------
// wxTextDataObject
// ----------------------------------------------------------------------------

#ifdef wxNEEDS_UTF8_FOR_TEXT_DATAOBJ

// FIXME-UTF8: we should be able to merge wchar_t and UTF-8 versions once we
//             have a way to get UTF-8 string (and its length) in both builds
//             without loss of efficiency (i.e. extra buffer copy/strlen call)

#if wxUSE_UNICODE_WCHAR

static inline wxMBConv& GetConv(const wxDataFormat& format)
{
    // use UTF8 for wxDF_UNICODETEXT and UCS4 for wxDF_TEXT
    return format == wxDF_UNICODETEXT ? wxConvUTF8 : wxConvLibc;
}

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    wxCharBuffer buffer = GetConv(format).cWX2MB( GetText().c_str() );

    return buffer ? strlen( buffer ) : 0;
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( !buf )
        return false;

    wxCharBuffer buffer = GetConv(format).cWX2MB( GetText().c_str() );
    if ( !buffer )
        return false;

    memcpy( (char*) buf, buffer, GetDataSize(format) );
    // strcpy( (char*) buf, buffer );

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t WXUNUSED(len), const void *buf)
{
    if ( buf == NULL )
        return false;

    wxWCharBuffer buffer = GetConv(format).cMB2WX( (const char*)buf );

    SetText( buffer );

    return true;
}

#else // wxUSE_UNICODE_UTF8

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        return m_text.utf8_length();
    }
    else // wxDF_TEXT
    {
        const wxCharBuffer buf(wxConvLocal.cWC2MB(m_text.wc_str()));
        return buf ? strlen(buf) : 0;
    }
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( !buf )
        return false;

    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        memcpy(buf, m_text.utf8_str(), m_text.utf8_length());
    }
    else // wxDF_TEXT
    {
        const wxCharBuffer bufLocal(wxConvLocal.cWC2MB(m_text.wc_str()));
        if ( !bufLocal )
            return false;

        memcpy(buf, bufLocal, strlen(bufLocal));
    }

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t len, const void *buf_)
{
    const char * const buf = static_cast<const char *>(buf_);

    if ( buf == NULL )
        return false;

    if ( format == wxDF_UNICODETEXT || wxLocaleIsUtf8 )
    {
        // normally the data is in UTF-8 so we could use FromUTF8Unchecked()
        // but it's not absolutely clear what GTK+ does if the clipboard data
        // is not in UTF-8 so do an extra check for tranquility, it shouldn't
        // matter much if we lose a bit of performance when pasting from
        // clipboard
        m_text = wxString::FromUTF8(buf, len);
    }
    else // wxDF_TEXT, convert from current (non-UTF8) locale
    {
        m_text = wxConvLocal.cMB2WC(buf, len, NULL);
    }

    return true;
}

#endif // wxUSE_UNICODE_WCHAR/wxUSE_UNICODE_UTF8

#elif defined(wxNEEDS_UTF16_FOR_TEXT_DATAOBJ)

namespace
{

inline wxMBConv& GetConv(const wxDataFormat& format)
{
    static wxMBConvUTF16 s_UTF16Converter;

    return format == wxDF_UNICODETEXT ? static_cast<wxMBConv&>(s_UTF16Converter)
                                      : static_cast<wxMBConv&>(wxConvLocal);
}

} // anonymous namespace

size_t wxTextDataObject::GetDataSize(const wxDataFormat& format) const
{
    return GetConv(format).WC2MB(NULL, GetText().wc_str(), 0);
}

bool wxTextDataObject::GetDataHere(const wxDataFormat& format, void *buf) const
{
    if ( buf == NULL )
        return false;

    wxCharBuffer buffer(GetConv(format).cWX2MB(GetText().c_str()));

    memcpy(buf, buffer.data(), buffer.length());

    return true;
}

bool wxTextDataObject::SetData(const wxDataFormat& format,
                               size_t WXUNUSED(len),
                               const void *buf)
{
    if ( buf == NULL )
        return false;

    SetText(GetConv(format).cMB2WX(static_cast<const char*>(buf)));

    return true;
}

#else // !wxNEEDS_UTF{8,16}_FOR_TEXT_DATAOBJ

size_t wxTextDataObject::GetDataSize() const
{
    return GetTextLength() * sizeof(wxChar);
}

bool wxTextDataObject::GetDataHere(void *buf) const
{
    // NOTE: use wxTmemcpy() instead of wxStrncpy() to allow
    //       retrieval of strings with embedded NULLs
    wxTmemcpy( (wxChar*)buf, GetText().c_str(), GetTextLength() );

    return true;
}

bool wxTextDataObject::SetData(size_t len, const void *buf)
{
    SetText( wxString((const wxChar*)buf, len/sizeof(wxChar)) );

    return true;
}

#endif // different wxTextDataObject implementations

// ----------------------------------------------------------------------------
// wxCustomDataObject
// ----------------------------------------------------------------------------

wxCustomDataObject::wxCustomDataObject(const wxDataFormat& format)
    : wxDataObjectSimple(format)
{
    m_data = NULL;
    m_size = 0;
}

wxCustomDataObject::~wxCustomDataObject()
{
    Free();
}

void wxCustomDataObject::TakeData(size_t size, void *data)
{
    Free();

    m_size = size;
    m_data = data;
}

void *wxCustomDataObject::Alloc(size_t size)
{
    return (void *)new char[size];
}

void wxCustomDataObject::Free()
{
    delete [] (char*)m_data;
    m_size = 0;
    m_data = NULL;
}

size_t wxCustomDataObject::GetDataSize() const
{
    return GetSize();
}

bool wxCustomDataObject::GetDataHere(void *buf) const
{
    if ( buf == NULL )
        return false;

    void *data = GetData();
    if ( data == NULL )
        return false;

    memcpy( buf, data, GetSize() );

    return true;
}

bool wxCustomDataObject::SetData(size_t size, const void *buf)
{
    Free();

    m_data = Alloc(size);
    if ( m_data == NULL )
        return false;

    m_size = size;
    memcpy( m_data, buf, m_size );

    return true;
}

// ============================================================================
// some common dnd related code
// ============================================================================

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

// ----------------------------------------------------------------------------
// wxTextDropTarget
// ----------------------------------------------------------------------------

// NB: we can't use "new" in ctor initializer lists because this provokes an
//     internal compiler error with VC++ 5.0 (hey, even gcc compiles this!),
//     so use SetDataObject() instead

wxTextDropTarget::wxTextDropTarget()
{
    SetDataObject(new wxTextDataObject);
}

wxDragResult wxTextDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxTextDataObject *dobj = (wxTextDataObject *)m_dataObject;
    return OnDropText( x, y, dobj->GetText() ) ? def : wxDragNone;
}

// ----------------------------------------------------------------------------
// wxFileDropTarget
// ----------------------------------------------------------------------------

wxFileDropTarget::wxFileDropTarget()
{
    SetDataObject(new wxFileDataObject);
}

wxDragResult wxFileDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult def)
{
    if ( !GetData() )
        return wxDragNone;

    wxFileDataObject *dobj = (wxFileDataObject *)m_dataObject;
    return OnDropFiles( x, y, dobj->GetFilenames() ) ? def : wxDragNone;
}

#endif // wxUSE_DRAG_AND_DROP

#endif // wxUSE_DATAOBJ
