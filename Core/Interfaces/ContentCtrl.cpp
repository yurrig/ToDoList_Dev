// ContentCtrl.cpp: implementation of the CContentCtrl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ContentCtrl.h"
#include "uithemefile.h"

#include "..\shared\autoflag.h"
#include "..\shared\misc.h"
#include "..\shared\binarydata.h"
#include "..\shared\tooltipctrlex.h"
#include "..\shared\dialoghelper.h"

#include "..\Interfaces\IContentControl.h"
#include "..\Interfaces\ISpellCheck.h"
#include "..\Interfaces\uitheme.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

BOOL CONTENTFORMAT::FormatIsText() const
{
	GUID id;
	return (Misc::GuidFromString(*this, id) == FALSE);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CContentCtrl::CContentCtrl(IContentControl* pContentCtrl) : 
	m_pContentCtrl(pContentCtrl), m_bSettingContent(FALSE)
{

}

CContentCtrl::~CContentCtrl()
{
}

CContentCtrl::operator HWND() const
{ 
	return GetSafeHwnd();
}

HWND CContentCtrl::GetSafeHwnd() const
{ 
	if (m_pContentCtrl)
		return m_pContentCtrl->GetHwnd();
	
	// else
	return NULL;
}

BOOL CContentCtrl::ProcessMessage(MSG* pMsg)
{
	if (m_pContentCtrl)
	{
		// Always allow tooltip handling thru
		m_pContentCtrl->FilterToolTipMessage(pMsg);

		// only process if we have the focus
		if (HasFocus())
		{
			// don't forward tab messages if screen reader is active
			BOOL bTabAndScreenReader = ((pMsg->message == WM_KEYDOWN) && (pMsg->wParam == VK_TAB) && Misc::IsScreenReaderActive());
			
			if (!bTabAndScreenReader)
				return m_pContentCtrl->ProcessMessage(pMsg);
		}
	}

	return FALSE;
}

BOOL CContentCtrl::Attach(IContentControl* pContentCtrl)
{
	ASSERT (pContentCtrl && pContentCtrl->GetHwnd());

	if (pContentCtrl && pContentCtrl->GetHwnd())
	{
		// release existing control
		if (m_pContentCtrl)
			::DestroyWindow(m_pContentCtrl->GetHwnd());

		m_pContentCtrl = pContentCtrl;
		m_sTypeID = m_pContentCtrl->GetTypeID();
		
		return TRUE;
	}

	// else
	return FALSE;
}

void CContentCtrl::SetUITheme(const CUIThemeFile& theme)
{
	if (m_pContentCtrl)
	{
		// configure theme for this plugin
		CUIThemeFile themePlugin(theme);
		themePlugin.SetToolbarImageFile(GetTypeID());

		m_pContentCtrl->SetUITheme(&themePlugin);
	}
}

void CContentCtrl::SetContentFont(HFONT hFont)
{
	ASSERT(hFont);

	if (m_pContentCtrl)
		m_pContentCtrl->SetContentFont(hFont);
}

void CContentCtrl::SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const
{
	if (m_pContentCtrl && !Misc::IsEmpty(szKey))
	{
		CString sKey;
		sKey.Format(_T("%s\\%s"), szKey, GetTypeID());

		m_pContentCtrl->SavePreferences(pPrefs, sKey);
	}
}

void CContentCtrl::LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey, BOOL bAppOnly)
{
	if (m_pContentCtrl && !Misc::IsEmpty(szKey))
	{
		CString sKey;
		sKey.Format(_T("%s\\%s"), szKey, GetTypeID());
		
		m_pContentCtrl->LoadPreferences(pPrefs, sKey, (bAppOnly != FALSE));
	}
}

BOOL CContentCtrl::HasFocus() const
{
	if (m_pContentCtrl)
		return CDialogHelper::IsChildOrSame(GetSafeHwnd(), ::GetFocus());

	// else
	return FALSE;
}

void CContentCtrl::SetFocus()
{
	HWND hwndThis = GetSafeHwnd();

	if (::IsWindowEnabled(hwndThis))
		::SetFocus(hwndThis);
}

ISpellCheck* CContentCtrl::GetSpellCheckInterface()
{
	if (m_pContentCtrl)
		return m_pContentCtrl->GetSpellCheckInterface();

	// else
	return NULL;
}

BOOL CContentCtrl::Undo()
{
	if (m_pContentCtrl)
		return m_pContentCtrl->Undo();

	// else
	return FALSE;
}

BOOL CContentCtrl::Redo()
{
	if (m_pContentCtrl)
		return m_pContentCtrl->Redo();

	// else
	return FALSE;
}

BOOL CContentCtrl::FindReplaceAll(LPCTSTR szFind, LPCTSTR szReplace, BOOL bCaseSensitive, BOOL bWholeWord)
{
	if (m_pContentCtrl)
		return m_pContentCtrl->FindReplaceAll(szFind, szReplace, (bCaseSensitive != FALSE), (bWholeWord != FALSE));

	// else
	return FALSE;
}

BOOL CContentCtrl::ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags)
{
	if (GetSafeHwnd())
		return CWnd::ModifyStyle(*this, dwRemove, dwAdd, nFlags);

	// else
	return FALSE;
}

BOOL CContentCtrl::ModifyStyleEx(DWORD dwRemove, DWORD dwAdd, UINT nFlags)
{
	if (GetSafeHwnd())
		return CWnd::ModifyStyleEx(*this, dwRemove, dwAdd, nFlags);

	// else
	return FALSE;
}

LRESULT CContentCtrl::SendMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_pContentCtrl)
		return ::SendMessage(*this, message, wParam, lParam);

	return 0L;
}

BOOL CContentCtrl::SetReadOnly(BOOL bReadOnly)
{
	if (m_pContentCtrl)
	{
		m_pContentCtrl->SetReadOnly(bReadOnly != FALSE);
		return TRUE;
	}

	return FALSE;
}

BOOL CContentCtrl::EnableWindow(BOOL bEnable)
{
	if (m_pContentCtrl)
	{
		m_pContentCtrl->Enable(bEnable != FALSE);
		return TRUE;
	}

	return FALSE;
}

void CContentCtrl::MoveWindow(const CRect& rect, BOOL bRepaint)
{
	::MoveWindow(GetSafeHwnd(), rect.left, rect.top, rect.Width(), rect.Height(), bRepaint);
}

BOOL CContentCtrl::PostMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_pContentCtrl)
		return ::PostMessage(*this, message, wParam, lParam);

	return FALSE;
}

int CContentCtrl::GetContent(unsigned char* pContent) const
{
	if (m_pContentCtrl)
		return m_pContentCtrl->GetContent(pContent);

	// else
	return 0;
}

BOOL CContentCtrl::SetContent(const unsigned char* pContent, int nLength, BOOL bResetSelection)
{
	CAutoFlag af(m_bSettingContent, TRUE);

	if (m_pContentCtrl)
		return m_pContentCtrl->SetContent(pContent, nLength, (bResetSelection != FALSE));

	// else
	return false;
}

BOOL CContentCtrl::SetContent(const CBinaryData& content, BOOL bResetSelection)
{
	return SetContent(content.Get(), content.GetLength(), bResetSelection);
}

int CContentCtrl::GetContent(CBinaryData& content) const
{
	int nLenBytes = 0;
	content.Empty();
	
	if (m_pContentCtrl)
	{
		nLenBytes = m_pContentCtrl->GetContent(NULL);
		
		if (nLenBytes) // excludes NULL
		{
			unsigned char* szContent = content.GetBuffer(nLenBytes);
			nLenBytes = m_pContentCtrl->GetContent(szContent);
			content.ReleaseBuffer(nLenBytes);
		}
	}
	
	return nLenBytes;
}

int CContentCtrl::GetTextContent(CString& sContent) const
{
	int nLen = 0;
	sContent.Empty();

	if (m_pContentCtrl)
	{
		nLen = m_pContentCtrl->GetTextContent(NULL);

		if (nLen)
		{
			LPTSTR szContent = sContent.GetBuffer(nLen + 1);
			nLen = m_pContentCtrl->GetTextContent(szContent, nLen + 1);
			sContent.ReleaseBuffer(nLen);
		}
	}

	// else
	return nLen;
}

BOOL CContentCtrl::SetTextContent(LPCTSTR szContent, BOOL bResetSelection)
{
	CAutoFlag af(m_bSettingContent, TRUE);

	if (m_pContentCtrl)
		return m_pContentCtrl->SetTextContent(szContent, (bResetSelection != FALSE));

	// else
	return FALSE;
}

BOOL CContentCtrl::InsertTextContent(LPCTSTR szContent, BOOL bAtEnd)
{
	CAutoFlag af(m_bSettingContent, TRUE);

	if (m_pContentCtrl)
		return m_pContentCtrl->InsertTextContent(szContent, (bAtEnd != FALSE));

	// else
	return FALSE;
}

LPCTSTR CContentCtrl::GetTypeID() const
{
	return m_sTypeID;
}

BOOL CContentCtrl::IsFormat(const CONTENTFORMAT& cf) const
{
	return (!cf.IsEmpty() && cf == GetContentFormat());
}

CONTENTFORMAT CContentCtrl::GetContentFormat() const
{
	return GetTypeID();
}
