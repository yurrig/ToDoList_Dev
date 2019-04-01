// KanbanTreeList.cpp: implementation of the CKanbanTreeList class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "KanbanCtrl.h"
#include "KanbanListCtrl.h"
#include "KanbanColors.h"
#include "KanbanMsg.h"

#include "..\shared\DialogHelper.h"
#include "..\shared\DateHelper.h"
#include "..\shared\holdredraw.h"
#include "..\shared\graphicsMisc.h"
#include "..\shared\autoflag.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\enstring.h"
#include "..\shared\localizer.h"
#include "..\shared\themed.h"
#include "..\shared\winclasses.h"
#include "..\shared\wclassdefines.h"
#include "..\shared\copywndcontents.h"
#include "..\shared\enbitmap.h"
#include "..\shared\DeferWndMove.h"

#include "..\Interfaces\iuiextension.h"
#include "..\Interfaces\ipreferences.h"
#include "..\Interfaces\TasklistSchemaDef.h"

#include <float.h> // for DBL_MAX
#include <math.h>  // for fabs()

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

#ifndef GET_WHEEL_DELTA_WPARAM
#	define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif 

#ifndef CDRF_SKIPPOSTPAINT
#	define CDRF_SKIPPOSTPAINT	(0x00000100)
#endif

//////////////////////////////////////////////////////////////////////

const UINT WM_KCM_SELECTTASK = (WM_APP+10); // WPARAM , LPARAM = Task ID

//////////////////////////////////////////////////////////////////////

const UINT IDC_LISTCTRL = 101;
const UINT IDC_HEADER	= 102;

//////////////////////////////////////////////////////////////////////

const int MIN_COL_WIDTH = GraphicsMisc::ScaleByDPIFactor(6);
const int HEADER_HEIGHT = GraphicsMisc::ScaleByDPIFactor(24);

//////////////////////////////////////////////////////////////////////

static CString EMPTY_STR;

//////////////////////////////////////////////////////////////////////

#define GET_KI_RET(id, ki, ret)	\
{								\
	if (id == 0) return ret;	\
	ki = GetKanbanItem(id);		\
	ASSERT(ki);					\
	if (ki == NULL) return ret;	\
}

#define GET_KI(id, ki)		\
{							\
	if (id == 0) return;	\
	ki = GetKanbanItem(id);	\
	ASSERT(ki);				\
	if (ki == NULL)	return;	\
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKanbanCtrl::CKanbanCtrl() 
	:
	m_bSortAscending(-1), 
	m_dwOptions(0),
	m_bReadOnly(FALSE),
	m_nNextColor(0),
	m_pSelectedList(NULL),
	m_nTrackAttribute(IUI_NONE),
	m_nSortBy(IUI_NONE),
	m_bSelectTasks(FALSE),
	m_bResizingHeader(FALSE),
	m_bSettingListFocus(FALSE),
	m_bSavingToImage(FALSE)
{

}

CKanbanCtrl::~CKanbanCtrl()
{
}

BEGIN_MESSAGE_MAP(CKanbanCtrl, CWnd)
	//{{AFX_MSG_MAP(CKanbanCtrlEx)
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_HEADER, OnHeaderCustomDraw)
	ON_NOTIFY(HDN_ITEMCHANGING, IDC_HEADER, OnHeaderItemChanging)
	ON_NOTIFY(TVN_BEGINDRAG, IDC_LISTCTRL, OnBeginDragListItem)
	ON_NOTIFY(TVN_SELCHANGED, IDC_LISTCTRL, OnListItemSelChange)
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_LISTCTRL, OnListEditLabel)
	ON_NOTIFY(NM_SETFOCUS, IDC_LISTCTRL, OnListSetFocus)
	ON_WM_SETFOCUS()
	ON_WM_SETCURSOR()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_KLCN_TOGGLETASKDONE, OnListToggleTaskDone)
	ON_MESSAGE(WM_KLCN_TOGGLETASKFLAG, OnListToggleTaskFlag)
	ON_MESSAGE(WM_KLCN_GETTASKICON, OnListGetTaskIcon)
	ON_MESSAGE(WM_KLCN_EDITTASKICON, OnListEditTaskIcon)
	ON_MESSAGE(WM_KCM_SELECTTASK, OnSelectTask)

END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////

BOOL CKanbanCtrl::Create(DWORD dwStyle, const RECT &rect, CWnd* pParentWnd, UINT nID)
{
	return CWnd::Create(NULL, NULL, dwStyle, rect, pParentWnd, nID);
}

int CKanbanCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_fonts.Initialise(*this);
	
	ModifyStyleEx(0, WS_EX_CONTROLPARENT, 0);

	if (!m_header.Create(HDS_FULLDRAG | /*HDS_DRAGDROP |*/ WS_CHILD | WS_VISIBLE, 
						 CRect(lpCreateStruct->x, lpCreateStruct->y, lpCreateStruct->cx, 50),
						 this, IDC_HEADER))
	{
		return -1;
	}

	return 0;
}

void CKanbanCtrl::FilterToolTipMessage(MSG* pMsg) 
{
	// List tooltips
	CKanbanColumnCtrl* pList = m_aListCtrls.HitTest(pMsg->pt);

	if (pList)
		pList->FilterToolTipMessage(pMsg);
}

bool CKanbanCtrl::ProcessMessage(MSG* pMsg) 
{
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		return (HandleKeyDown(pMsg->wParam, pMsg->lParam) != FALSE);
	}
	
	// all else
	return false;
}

BOOL CKanbanCtrl::SelectClosestAdjacentItemToSelection(int nAdjacentList)
{
	// Find the closest task at the currently
	// selected task's scrolled pos
	HTREEITEM hti = m_pSelectedList->GetSelectedItem();

	if (!hti)
	{
		ASSERT(0);
		return FALSE;
	}

	if ((nAdjacentList < 0) || (nAdjacentList > m_aListCtrls.GetSize()))
	{
		ASSERT(0);
		return FALSE;
	}

	CKanbanColumnCtrl* pAdjacentList = m_aListCtrls[nAdjacentList];

	if (!pAdjacentList->GetCount())
		return FALSE;

	// scroll into view first
	m_pSelectedList->ScrollToSelection();

	CRect rItem;
	VERIFY(m_pSelectedList->GetItemBounds(hti, &rItem));

	HTREEITEM htiClosest = pAdjacentList->HitTest(rItem.CenterPoint());

	if (!htiClosest)
		htiClosest = pAdjacentList->TCH().GetLastItem();

	SelectListCtrl(pAdjacentList, FALSE);
	ASSERT(m_pSelectedList == pAdjacentList);

	pAdjacentList->SelectItem(htiClosest, FALSE); // FALSE -> by keyboard
	pAdjacentList->UpdateWindow();
	
	return TRUE;
}

BOOL CKanbanCtrl::HandleKeyDown(WPARAM wp, LPARAM lp)
{
	switch (wp)
	{
	case VK_LEFT:
		if (m_pSelectedList->GetSelectedItem())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);

			for (int nList = (nSelList - 1); nList >= 0; nList--)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_HOME:
		if (m_pSelectedList->GetSelectedItem())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);

			for (int nList = 0; nList < nSelList; nList++)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_RIGHT:
		if (m_pSelectedList->GetSelectedItem())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);
			int nNumList = m_aListCtrls.GetSize();

			for (int nList = (nSelList + 1); nList < nNumList; nList++)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_END:
		if (m_pSelectedList->GetSelectedItem())
		{
			int nSelList = m_aListCtrls.Find(m_pSelectedList);
			int nNumList = m_aListCtrls.GetSize();

			for (int nList = (nNumList - 1); nList > nSelList; nList--)
			{
				if (SelectClosestAdjacentItemToSelection(nList))
					return TRUE;
			}
		}
		break;

	case VK_ESCAPE:
		// handle 'escape' during dragging
		return (CancelOperation() != FALSE);

	case VK_DELETE:
		if (m_pSelectedList && !m_pSelectedList->IsBacklog())
		{
			// For each of the selected tasks remove the attribute value(s) of the
			// selected list (column). Tasks having no values remaining are moved 
			// to the backlog
			CStringArray aListValues;
			VERIFY(m_pSelectedList->GetAttributeValues(aListValues));

			DWORD dwTaskID = GetSelectedTaskID();
			KANBANITEM* pKI = GetKanbanItem(dwTaskID);
			ASSERT(pKI);

			if (pKI)
				pKI->RemoveTrackedAttributeValues(m_sTrackAttribID, aListValues);

			// Notify parent of changes before altering the lists because we can't
			// guarantee that all the modified tasks will be in the same list afterwards
			NotifyParentAttibuteChange(dwTaskID);

			// Reset selected list before removing items to 
			// to prevent unwanted selection notifications
			CKanbanColumnCtrl* pList = m_pSelectedList;
			m_pSelectedList = NULL;

			if (pKI)
			{
				VERIFY(pList->DeleteTask(dwTaskID));

				if (!pKI->HasTrackedAttributeValues(m_sTrackAttribID))
				{
					CKanbanColumnCtrl* pBacklog = m_aListCtrls.GetBacklog();

					if (pBacklog)
						pBacklog->AddTask(*pKI, FALSE);
				}
			}

			// try to restore selection
			SelectTask(dwTaskID);
			return TRUE;
		}
		break;
	}

	// all else
	return FALSE;
}

BOOL CKanbanCtrl::SelectTask(DWORD dwTaskID)
{
	CAutoFlag af(m_bSelectTasks, TRUE);

	// Check for 'no change'
	DWORD dwSelID = GetSelectedTaskID();

	int nPrevSel = m_aListCtrls.Find(m_pSelectedList), nNewSel = -1;

	if (m_pSelectedList && m_pSelectedList->FindTask(dwTaskID))
		nNewSel = nPrevSel;
	else
		nNewSel = m_aListCtrls.Find(dwTaskID);

	if ((nPrevSel != nNewSel) || (dwTaskID != dwSelID))
	{
		m_aListCtrls.SetSelectedList(NULL);

		if ((nNewSel == -1) || (dwTaskID == 0))
			return FALSE;

		// else
		SelectListCtrl(m_aListCtrls[nNewSel], FALSE);
		VERIFY(m_pSelectedList->SelectTask(dwTaskID));
	}

	ScrollToSelectedTask();
	return TRUE;
}

DWORD CKanbanCtrl::GetSelectedTaskID() const
{
	const CKanbanColumnCtrl* pList = GetSelListCtrl();

	if (pList)
		return pList->GetSelectedTaskID();

	// else
	return 0;
}

CKanbanColumnCtrl* CKanbanCtrl::GetSelListCtrl()
{
	ASSERT((m_pSelectedList == NULL) || Misc::HasT(m_aListCtrls, m_pSelectedList));

	if (!m_pSelectedList && m_aListCtrls.GetSize())
		m_pSelectedList = m_aListCtrls[0];

	return m_pSelectedList;
}

const CKanbanColumnCtrl* CKanbanCtrl::GetSelListCtrl() const
{
	if (m_pSelectedList)
	{
		return m_pSelectedList;
	}
	else if (m_aListCtrls.GetSize())
	{
		return m_aListCtrls[0];
	}

	// else
	return NULL;
}

BOOL CKanbanCtrl::SelectTask(IUI_APPCOMMAND nCmd, const IUISELECTTASK& select)
{
	CKanbanColumnCtrl* pList = NULL;
	HTREEITEM htiStart = NULL;
	BOOL bForwards = TRUE;

	switch (nCmd)
	{
	case IUI_SELECTFIRSTTASK:
		pList = m_aListCtrls.GetFirstNonEmpty();

		if (pList)
			htiStart = pList->TCH().GetFirstItem();
		break;

	case IUI_SELECTNEXTTASK:
		pList = m_pSelectedList;

		if (pList)
			htiStart = pList->GetNextSiblingItem(pList->GetSelectedItem());;
		break;

	case IUI_SELECTNEXTTASKINCLCURRENT:
		pList = m_pSelectedList;

		if (pList)
			htiStart = pList->GetSelectedItem();
		break;

	case IUI_SELECTPREVTASK:
		pList = m_pSelectedList;
		bForwards = FALSE;

		if (pList)
			htiStart = pList->GetPrevSiblingItem(pList->GetSelectedItem());
		break;

	case IUI_SELECTLASTTASK:
		pList = m_aListCtrls.GetLastNonEmpty();
		bForwards = FALSE;

		if (pList)
			htiStart = pList->TCH().GetLastItem();
		break;

	default:
		ASSERT(0);
		break;
	}

	if (pList)
	{
		const CKanbanColumnCtrl* pStartList = pList;
		HTREEITEM hti = htiStart;
		
		do
		{
			hti = pList->FindTask(select, bForwards, hti);

			if (hti)
			{
				SelectListCtrl(pList, FALSE);
				return pList->SelectItem(hti, FALSE);
			}

			// else
			pList = GetNextListCtrl(pList, bForwards, TRUE);
			hti = (bForwards ? pList->TCH().GetFirstItem() : pList->TCH().GetLastItem());
		}
		while (pList != pStartList);
	}

	// all else
	return false;
}

BOOL CKanbanCtrl::HasFocus() const
{
	return CDialogHelper::IsChildOrSame(GetSafeHwnd(), ::GetFocus());
}

void CKanbanCtrl::UpdateTasks(const ITaskList* pTaskList, IUI_UPDATETYPE nUpdate, const CSet<IUI_ATTRIBUTE>& attrib)
{
	ASSERT(GetSafeHwnd());

	const ITASKLISTBASE* pTasks = GetITLInterface<ITASKLISTBASE>(pTaskList, IID_TASKLISTBASE);

	if (pTasks == NULL)
	{
		ASSERT(0);
		return;
	}

	// always cancel any ongoing operation
	CancelOperation();

	BOOL bResort = FALSE;
	
	switch (nUpdate)
	{
	case IUI_ALL:
		RebuildData(pTasks, attrib);
 		RebuildListCtrls(TRUE, TRUE);
		break;
		
	case IUI_NEW:
	case IUI_EDIT:
		{
 			// update the task(s)
			BOOL bChange = UpdateGlobalAttributeValues(pTasks, attrib);
			bChange |= UpdateData(pTasks, pTasks->GetFirstTask(), attrib, TRUE);

			if (bChange)
			{
				RebuildListCtrls(TRUE, TRUE);
			}
			else if (UpdateNeedsItemHeightRefresh(attrib))
			{
				m_aListCtrls.RefreshItemLineHeights();
			}
			else
			{
				m_aListCtrls.Redraw(FALSE);
			}
		}
		break;
		
	case IUI_DELETE:
		RemoveDeletedTasks(pTasks);
		break;
		
	default:
		ASSERT(0);
	}
}

BOOL CKanbanCtrl::UpdateNeedsItemHeightRefresh(const CSet<IUI_ATTRIBUTE>& attrib) const
{
	if (HasOption(KBCF_HIDEEMPTYATTRIBUTES))
	{
		int nAtt = m_aDisplayAttrib.GetSize();

		while (nAtt--)
		{
			if (attrib.Has(m_aDisplayAttrib[nAtt]))
				return TRUE;
		}
	}

	return FALSE;
}

int CKanbanCtrl::GetTaskAllocTo(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskAllocatedToCount(hTask);
	
	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskAllocatedTo(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskAllocatedTo(hTask, nItem));
		}
	}	
	
	return aValues.GetSize();
}

int CKanbanCtrl::GetTaskCategories(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskCategoryCount(hTask);

	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskCategory(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskCategory(hTask, nItem));
		}
	}	

	return aValues.GetSize();
}

int CKanbanCtrl::GetTaskTags(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CStringArray& aValues)
{
	aValues.RemoveAll();
	int nItem = pTasks->GetTaskTagCount(hTask);

	if (nItem > 0)
	{
		if (nItem == 1)
		{
			aValues.Add(pTasks->GetTaskTag(hTask, 0));
		}
		else
		{
			while (nItem--)
				aValues.InsertAt(0, pTasks->GetTaskTag(hTask, nItem));
		}
	}	

	return aValues.GetSize();
}

// External interface
BOOL CKanbanCtrl::WantEditUpdate(IUI_ATTRIBUTE nAttrib) const
{
	switch (nAttrib)
	{
	case IUI_ALLOCBY:
	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_COLOR:
	case IUI_COST:
	case IUI_CREATIONDATE:
	case IUI_CREATEDBY:
	case IUI_CUSTOMATTRIB:
	case IUI_DONEDATE:
	case IUI_DUEDATE:
	case IUI_EXTERNALID:
	case IUI_FILEREF:
	case IUI_FLAG:
	case IUI_ICON:
	case IUI_LASTMOD:
	case IUI_PERCENT:
	case IUI_PRIORITY:
	case IUI_RECURRENCE:
	case IUI_RISK:
	case IUI_STARTDATE:
	case IUI_STATUS:
	case IUI_SUBTASKDONE:
	case IUI_TAGS:
	case IUI_TASKNAME:
	case IUI_TIMEEST:
	case IUI_TIMESPENT:
	case IUI_VERSION:
		return TRUE;
	}
	
	// all else 
	return (nAttrib == IUI_ALL);
}

// External interface
BOOL CKanbanCtrl::WantSortUpdate(IUI_ATTRIBUTE nAttrib) const
{
	switch (nAttrib)
	{
	case IUI_NONE:
		return HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS);
	}

	// all else
	return WantEditUpdate(nAttrib);
}

BOOL CKanbanCtrl::RebuildData(const ITASKLISTBASE* pTasks, const CSet<IUI_ATTRIBUTE>& attrib)
{
	// Rebuild global attribute value lists
	m_mapAttributeValues.RemoveAll();
	m_aCustomAttribDefs.RemoveAll();

	UpdateGlobalAttributeValues(pTasks, attrib);

	// Rebuild data
	m_data.RemoveAll();

	return AddTaskToData(pTasks, pTasks->GetFirstTask(), 0, attrib, TRUE);
}

BOOL CKanbanCtrl::AddTaskToData(const ITASKLISTBASE* pTasks, HTASKITEM hTask, DWORD dwParentID, const CSet<IUI_ATTRIBUTE>& attrib, BOOL bAndSiblings)
{
	if (!hTask)
		return FALSE;

	// Not interested in references
	if (!pTasks->IsTaskReference(hTask))
	{
		DWORD dwTaskID = pTasks->GetTaskID(hTask);

		KANBANITEM* pKI = m_data.NewItem(dwTaskID, pTasks->GetTaskTitle(hTask));
		ASSERT(pKI);
	
		if (!pKI)
			return FALSE;

		pKI->bDone = pTasks->IsTaskDone(hTask);
		pKI->bGoodAsDone = pTasks->IsTaskGoodAsDone(hTask);
		pKI->bParent = pTasks->IsTaskParent(hTask);
		pKI->dwParentID = dwParentID;
		pKI->bLocked = pTasks->IsTaskLocked(hTask, true);
		pKI->bHasIcon = !Misc::IsEmpty(pTasks->GetTaskIcon(hTask));
		pKI->bFlag = (pTasks->IsTaskFlagged(hTask, false) ? TRUE : FALSE);
		pKI->nPosition = pTasks->GetTaskPosition(hTask);

		pKI->SetColor(pTasks->GetTaskTextColor(hTask));

		LPCWSTR szSubTaskDone = pTasks->GetTaskSubtaskCompletion(hTask);
		pKI->bSomeSubtaskDone = (!Misc::IsEmpty(szSubTaskDone) && (szSubTaskDone[0] != '0'));
	
		// Path is parent's path + parent's name
		if (dwParentID)
		{
			const KANBANITEM* pKIParent = m_data.GetItem(dwParentID);
			ASSERT(pKIParent);

			if (pKIParent->sPath.IsEmpty())
				pKI->sPath = pKIParent->sTitle;
			else
				pKI->sPath = pKIParent->sPath + '\\' + pKIParent->sTitle;

			pKI->nLevel = (pKIParent->nLevel + 1);
		}
		else
		{
			pKI->nLevel = 0;
		}

		// trackable attributes
		CStringArray aValues;

		if (GetTaskCategories(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_CATEGORY, aValues);

		if (GetTaskAllocTo(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_ALLOCTO, aValues);

		if (GetTaskTags(pTasks, hTask, aValues))
			pKI->SetTrackedAttributeValues(IUI_TAGS, aValues);
	
		pKI->SetTrackedAttributeValue(IUI_STATUS, pTasks->GetTaskStatus(hTask));
		pKI->SetTrackedAttributeValue(IUI_ALLOCBY, pTasks->GetTaskAllocatedBy(hTask));
		pKI->SetTrackedAttributeValue(IUI_VERSION, pTasks->GetTaskVersion(hTask));
		pKI->SetTrackedAttributeValue(IUI_PRIORITY, pTasks->GetTaskPriority(hTask, FALSE));
		pKI->SetTrackedAttributeValue(IUI_RISK, pTasks->GetTaskRisk(hTask, FALSE));

		// custom attributes
		int nCust = pTasks->GetCustomAttributeCount();

		while (nCust--)
		{
			CString sCustID(pTasks->GetCustomAttributeID(nCust));
			CString sCustValue(pTasks->GetTaskCustomAttributeData(hTask, sCustID, true));

			CStringArray aCustValues;
			Misc::Split(sCustValue, aCustValues, '+');

			pKI->SetTrackedAttributeValues(sCustID, aCustValues);

			// Add to global attribute values
			CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(sCustID);
			ASSERT(pValues);
			
			pValues->AddValues(aCustValues);
		}

		// Other display-only attributes
		UpdateItemDisplayAttributes(pKI, pTasks, hTask, attrib);

		// first child
		AddTaskToData(pTasks, pTasks->GetFirstTask(hTask), dwTaskID, attrib, TRUE);
	}

	// Siblings NON-RECURSIVELY
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);

		while (hSibling)
		{
			// FALSE == don't recurse on siblings
			AddTaskToData(pTasks, hSibling, dwParentID, attrib, FALSE);
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}

	return TRUE;
}

BOOL CKanbanCtrl::UpdateData(const ITASKLISTBASE* pTasks, HTASKITEM hTask, const CSet<IUI_ATTRIBUTE>& attrib, BOOL bAndSiblings)
{
	if (hTask == NULL)
		return FALSE;

	// Not interested in references
	if (pTasks->IsTaskReference(hTask))
		return FALSE; 

	// handle task if not NULL (== root)
	BOOL bChange = FALSE;
	DWORD dwTaskID = pTasks->GetTaskID(hTask);

	if (dwTaskID)
	{
		// Can be a new task
		if (!HasKanbanItem(dwTaskID))
		{
			bChange = AddTaskToData(pTasks, hTask, pTasks->GetTaskParentID(hTask), attrib, FALSE);
		}
		else
		{
			KANBANITEM* pKI = NULL;
			GET_KI_RET(dwTaskID, pKI, FALSE);

			if (attrib.Has(IUI_TASKNAME))
				pKI->sTitle = pTasks->GetTaskTitle(hTask);
			
			if (attrib.Has(IUI_DONEDATE))
			{
				BOOL bDone = pTasks->IsTaskDone(hTask);
				BOOL bGoodAsDone = pTasks->IsTaskGoodAsDone(hTask);

				if ((pKI->bDone != bDone) || (pKI->bGoodAsDone != bGoodAsDone))
				{
					pKI->bDone = bDone;
					pKI->bGoodAsDone = bGoodAsDone;
				}
			}

			if (attrib.Has(IUI_SUBTASKDONE))
			{
				LPCWSTR szSubTaskDone = pTasks->GetTaskSubtaskCompletion(hTask);
				pKI->bSomeSubtaskDone = (!Misc::IsEmpty(szSubTaskDone) && (szSubTaskDone[0] != '0'));
			}

			if (attrib.Has(IUI_ICON))
				pKI->bHasIcon = !Misc::IsEmpty(pTasks->GetTaskIcon(hTask));

			if (attrib.Has(IUI_FLAG))
				pKI->bFlag = (pTasks->IsTaskFlagged(hTask, true) ? TRUE : FALSE);
			
			// Trackable attributes
			CStringArray aValues;

			if (attrib.Has(IUI_ALLOCTO))
			{
				GetTaskAllocTo(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_ALLOCTO, aValues);
			}

			if (attrib.Has(IUI_CATEGORY))
			{
				GetTaskCategories(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_CATEGORY, aValues);
			}

			if (attrib.Has(IUI_TAGS))
			{
				GetTaskTags(pTasks, hTask, aValues);
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_TAGS, aValues);
			}

			if (attrib.Has(IUI_ALLOCBY))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_ALLOCBY, pTasks->GetTaskAllocatedBy(hTask));

			if (attrib.Has(IUI_STATUS))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_STATUS, pTasks->GetTaskStatus(hTask));

			if (attrib.Has(IUI_VERSION))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_VERSION, pTasks->GetTaskVersion(hTask));

			if (attrib.Has(IUI_PRIORITY))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_PRIORITY, pTasks->GetTaskPriority(hTask, true));

			if (attrib.Has(IUI_RISK))
				bChange |= UpdateTrackableTaskAttribute(pKI, IUI_RISK, pTasks->GetTaskRisk(hTask, true));

			if (attrib.Has(IUI_CUSTOMATTRIB))
			{
				int nID = m_aCustomAttribDefs.GetSize();

				while (nID--)
				{
					KANBANCUSTOMATTRIBDEF& def = m_aCustomAttribDefs[nID];

					CString sValue(pTasks->GetTaskCustomAttributeData(hTask, def.sAttribID, true));
					CStringArray aValues;

					if (!sValue.IsEmpty())
					{
						if (Misc::Split(sValue, aValues, '+') > 1)
							def.bMultiValue = TRUE;
					}

					if (UpdateTrackableTaskAttribute(pKI, def.sAttribID, aValues))
					{
						// Add to global values
						CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(def.sAttribID);
						ASSERT(pValues);
						
						pValues->AddValues(aValues);
						bChange = TRUE;
					}
				}
			}

			// other display-only attributes
			UpdateItemDisplayAttributes(pKI, pTasks, hTask, attrib);
			
			// always update colour because it can change for so many reasons
			pKI->SetColor(pTasks->GetTaskTextColor(hTask));

			// Always update lock state
			pKI->bLocked = pTasks->IsTaskLocked(hTask, true);
		}
	}
		
	// children
	if (UpdateData(pTasks, pTasks->GetFirstTask(hTask), attrib, TRUE))
		bChange = TRUE;

	// handle siblings WITHOUT RECURSION
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);
		
		while (hSibling)
		{
			// FALSE == not siblings
			if (UpdateData(pTasks, hSibling, attrib, FALSE))
				bChange = TRUE;
			
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}
	
	return bChange;
}

void CKanbanCtrl::UpdateItemDisplayAttributes(KANBANITEM* pKI, const ITASKLISTBASE* pTasks, HTASKITEM hTask, const CSet<IUI_ATTRIBUTE>& attrib)
{
	time64_t tDate = 0;
	
	if (attrib.Has(IUI_TIMEEST))
		pKI->dTimeEst = pTasks->GetTaskTimeEstimate(hTask, pKI->nTimeEstUnits, true);
	
	if (attrib.Has(IUI_TIMESPENT))
		pKI->dTimeSpent = pTasks->GetTaskTimeSpent(hTask, pKI->nTimeSpentUnits, true);
	
	if (attrib.Has(IUI_COST))
		pKI->dCost = pTasks->GetTaskCost(hTask, true);
	
	if (attrib.Has(IUI_CREATEDBY))
		pKI->sCreatedBy = pTasks->GetTaskCreatedBy(hTask);
	
	if (attrib.Has(IUI_CREATIONDATE))
		pKI->dtCreate = pTasks->GetTaskCreationDate(hTask);
	
	if (attrib.Has(IUI_DONEDATE) && pTasks->GetTaskDoneDate64(hTask, tDate))
		pKI->dtDone = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_DUEDATE) && pTasks->GetTaskDueDate64(hTask, true, tDate))
		pKI->dtDue = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_STARTDATE) && pTasks->GetTaskStartDate64(hTask, true, tDate))
		pKI->dtStart = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_LASTMOD) && pTasks->GetTaskLastModified64(hTask, tDate))
		pKI->dtLastMod = CDateHelper::GetDate(tDate);
	
	if (attrib.Has(IUI_PERCENT))
		pKI->nPercent = pTasks->GetTaskPercentDone(hTask, true);
	
	if (attrib.Has(IUI_EXTERNALID))
		pKI->sExternalID = pTasks->GetTaskExternalID(hTask);
	
	if (attrib.Has(IUI_RECURRENCE))
		pKI->sRecurrence = pTasks->GetTaskAttribute(hTask, TDL_TASKRECURRENCE);

	if (attrib.Has(IUI_FILEREF) && pTasks->GetTaskFileLinkCount(hTask))
	{
		pKI->sFileRef = pTasks->GetTaskFileLink(hTask, 0);

		// Get the shortest meaningful bit because of space constraints
		if (FileMisc::IsPath(pKI->sFileRef))
			pKI->sFileRef = FileMisc::GetFileNameFromPath(pKI->sFileRef);
	}
}

BOOL CKanbanCtrl::UpdateGlobalAttributeValues(const ITASKLISTBASE* pTasks, const CSet<IUI_ATTRIBUTE>& attrib)
{
	BOOL bChange = FALSE;

	if (attrib.Has(IUI_STATUS))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_STATUS);
	
	if (attrib.Has(IUI_ALLOCTO))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_ALLOCTO);
	
	if (attrib.Has(IUI_CATEGORY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_CATEGORY);
	
	if (attrib.Has(IUI_ALLOCBY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_ALLOCBY);
	
	if (attrib.Has(IUI_TAGS))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_TAGS);
	
	if (attrib.Has(IUI_VERSION))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_VERSION);
	
	if (attrib.Has(IUI_PRIORITY))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_PRIORITY);
	
	if (attrib.Has(IUI_RISK))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_RISK);
	
	if (attrib.Has(IUI_CUSTOMATTRIB))
		bChange |= UpdateGlobalAttributeValues(pTasks, IUI_CUSTOMATTRIB);
	
	return bChange;
}

BOOL CKanbanCtrl::UpdateGlobalAttributeValues(const ITASKLISTBASE* pTasks, IUI_ATTRIBUTE nAttribute)
{
	switch (nAttribute)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		{
			CString sAttribID(KANBANITEM::GetAttributeID(nAttribute));

			// create once only
			if (!m_mapAttributeValues.HasMapping(sAttribID))
			{
				CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(sAttribID);
				ASSERT(pValues);

				for (int nItem = 0; nItem <= 10; nItem++)
				{
					CString sValue(Misc::Format(nItem));
					pValues->SetAt(sValue, sValue);
				}
				
				// Add backlog item
				pValues->AddValue(EMPTY_STR);
			}
		}
		break;
		
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_TAGS:	
		{
			CString sXMLTag(GetXMLTag(nAttribute)); 
			CString sAttribID(KANBANITEM::GetAttributeID(nAttribute));

			CStringArray aNewValues;
			int nValue = pTasks->GetAttributeCount(sXMLTag);

			while (nValue--)
			{
				CString sValue(pTasks->GetAttributeItem(sXMLTag, nValue));

				if (!sValue.IsEmpty())
					aNewValues.Add(sValue);
			}

			return UpdateGlobalAttributeValues(sAttribID, aNewValues);
		}
		break;
		
	case IUI_CUSTOMATTRIB:
		{
			BOOL bChange = FALSE;
			int nCustom = pTasks->GetCustomAttributeCount();

			while (nCustom--)
			{
				// Save off each attribute ID
				if (pTasks->IsCustomAttributeEnabled(nCustom))
				{
					CString sAttribID(pTasks->GetCustomAttributeID(nCustom));
					CString sAttribName(pTasks->GetCustomAttributeLabel(nCustom));

					int nDef = m_aCustomAttribDefs.AddDefinition(sAttribID, sAttribName);

					// Add 'default' values to the map
					CKanbanValueMap* pDefValues = m_mapGlobalAttributeValues.GetAddMapping(sAttribID);
					ASSERT(pDefValues);

					pDefValues->RemoveAll();

					CString sListData = pTasks->GetCustomAttributeListData(nCustom);

					// 'Auto' list values follow 'default' list values
					//  separated by a TAB
					CString sDefData(sListData), sAutoData;
					Misc::Split(sDefData, sAutoData, '\t');

					CStringArray aDefValues;
					
					if (Misc::Split(sDefData, aDefValues, '\n'))
					{
						pDefValues->SetValues(aDefValues);

						if (aDefValues.GetSize() > 1)
							m_aCustomAttribDefs.SetMultiValue(nDef);
					}

					CStringArray aAutoValues;
					Misc::Split(sAutoData, aAutoValues, '\n');

					bChange |= UpdateGlobalAttributeValues(sAttribID, aAutoValues);
				}
			}

			return bChange;
		}
		break;
	}

	// all else
	return FALSE;
}

BOOL CKanbanCtrl::UpdateGlobalAttributeValues(LPCTSTR szAttribID, const CStringArray& aValues)
{
	CKanbanValueMap mapNewValues;
	mapNewValues.AddValues(aValues);

	// Add in Backlog value
	mapNewValues.AddValue(EMPTY_STR);

	// Merge in default values
	const CKanbanValueMap* pDefValues = m_mapGlobalAttributeValues.GetMapping(szAttribID);

	if (pDefValues)
		Misc::Append(*pDefValues, mapNewValues);

	// Check for changes
	CKanbanValueMap* pValues = m_mapAttributeValues.GetAddMapping(szAttribID);
	ASSERT(pValues);
	
	if (!Misc::MatchAll(mapNewValues, *pValues))
	{
		Misc::Copy(mapNewValues, *pValues);

		return IsTracking(szAttribID);
	}

	// all else
	return FALSE;
}

int CKanbanCtrl::GetTaskTrackedAttributeValues(DWORD dwTaskID, CStringArray& aValues) const
{
	ASSERT(!m_sTrackAttribID.IsEmpty());

	const KANBANITEM* pKI = GetKanbanItem(dwTaskID);
	ASSERT(pKI);

	if (pKI)
		pKI->GetTrackedAttributeValues(m_sTrackAttribID, aValues);
	else
		aValues.RemoveAll();
	
	return aValues.GetSize();
}

int CKanbanCtrl::GetAttributeValues(IUI_ATTRIBUTE nAttrib, CStringArray& aValues) const
{
	CString sAttribID(KANBANITEM::GetAttributeID(nAttrib));

	const CKanbanValueMap* pValues = m_mapAttributeValues.GetMapping(sAttribID);
	aValues.SetSize(pValues->GetCount());

	if (pValues)
	{
		POSITION pos = pValues->GetStartPosition();
		int nItem = 0;

		while (pos)
			pValues->GetNextValue(pos, aValues[nItem++]);
	}

	return aValues.GetSize();
}

int CKanbanCtrl::GetAttributeValues(CKanbanAttributeValueMap& mapValues) const
{
	CString sAttribID;
	CKanbanValueMap* pValues = NULL;
	POSITION pos = m_mapAttributeValues.GetStartPosition();

	while (pos)
	{
		m_mapAttributeValues.GetNextAssoc(pos, sAttribID, pValues);
		ASSERT(pValues);

		CKanbanValueMap* pCopyValues = mapValues.GetAddMapping(sAttribID);
		ASSERT(pCopyValues);

		Misc::Copy(*pValues, *pCopyValues);
	}

	// Append default values
	pos = m_mapGlobalAttributeValues.GetStartPosition();

	while (pos)
	{
		m_mapGlobalAttributeValues.GetNextAssoc(pos, sAttribID, pValues);
		ASSERT(pValues);

		CKanbanValueMap* pCopyValues = mapValues.GetAddMapping(sAttribID);
		ASSERT(pCopyValues);

		Misc::Append(*pValues, *pCopyValues);
	}

	return mapValues.GetCount();
}

void CKanbanCtrl::LoadDefaultAttributeListValues(const IPreferences* pPrefs)
{
	m_mapGlobalAttributeValues.RemoveAll();

	LoadDefaultAttributeListValues(pPrefs, _T("ALLOCTO"),	_T("AllocToList"));
	LoadDefaultAttributeListValues(pPrefs, _T("ALLOCBY"),	_T("AllocByList"));
	LoadDefaultAttributeListValues(pPrefs, _T("STATUS"),	_T("StatusList"));
	LoadDefaultAttributeListValues(pPrefs, _T("CATEGORY"),	_T("CategoryList"));
	LoadDefaultAttributeListValues(pPrefs, _T("VERSION"),	_T("VersionList"));
	LoadDefaultAttributeListValues(pPrefs, _T("TAGS"),		_T("TagList"));

	if (m_nTrackAttribute != IUI_NONE)
		RebuildListCtrls(FALSE, FALSE);
}

void CKanbanCtrl::LoadDefaultAttributeListValues(const IPreferences* pPrefs, LPCTSTR szAttribID, LPCTSTR szSubKey)
{
	CKanbanValueMap* pMap = m_mapGlobalAttributeValues.GetAddMapping(szAttribID);
	ASSERT(pMap);

	CString sKey;
	sKey.Format(_T("Preferences\\%s"), szSubKey);

	int nCount = pPrefs->GetProfileInt(sKey, _T("ItemCount"), 0);
	
	// items
	for (int nItem = 0; nItem < nCount; nItem++)
	{
		CString sItemKey;
		sItemKey.Format(_T("Item%d"), nItem);

		CString sValue(pPrefs->GetProfileString(sKey, sItemKey));
		
		if (!sValue.IsEmpty())
			pMap->AddValue(sValue);
	}
}

CString CKanbanCtrl::GetXMLTag(IUI_ATTRIBUTE nAttrib)
{
	switch (nAttrib)
	{
	case IUI_ALLOCTO:	return TDL_TASKALLOCTO;
	case IUI_ALLOCBY:	return TDL_TASKALLOCBY;
	case IUI_STATUS:	return TDL_TASKSTATUS;
	case IUI_CATEGORY:	return TDL_TASKCATEGORY;
	case IUI_VERSION:	return TDL_TASKVERSION;
	case IUI_TAGS:		return TDL_TASKTAG;
		
	case IUI_CUSTOMATTRIB:
		ASSERT(0);
		break;
		
	default:
		ASSERT(0);
		break;
	}
	
	return EMPTY_STR;
}

BOOL CKanbanCtrl::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, int nNewValue)
{
#ifdef _DEBUG
	switch (nAttrib)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		break;

	default:
		ASSERT(0);
		break;
	}
#endif

	CString sValue; // empty

	if (nNewValue >= 0)
		sValue = Misc::Format(nNewValue);
	
	// else empty
	return UpdateTrackableTaskAttribute(pKI, nAttrib, sValue);
}

BOOL CKanbanCtrl::IsTrackedAttributeMultiValue() const
{
	switch (m_nTrackAttribute)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_ALLOCBY:
	case IUI_STATUS:
	case IUI_VERSION:
		return FALSE;

	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_TAGS:
		return TRUE;

	case IUI_CUSTOMATTRIB:
		{
			int nDef = m_aCustomAttribDefs.FindDefinition(m_sTrackAttribID);
			
			if (nDef != -1)
				return m_aCustomAttribDefs[nDef].bMultiValue;

		}
		break;
	}

	// all else
	ASSERT(0);
	return FALSE;
}

BOOL CKanbanCtrl::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, const CString& sNewValue)
{
	CStringArray aNewValues;

	switch (nAttrib)
	{
	case IUI_PRIORITY:
	case IUI_RISK:
		if (!sNewValue.IsEmpty())
			aNewValues.Add(sNewValue);
		break;

	case IUI_ALLOCBY:
	case IUI_STATUS:
	case IUI_VERSION:
		aNewValues.Add(sNewValue);
		break;

	default:
		ASSERT(0);
		break;
	}
	
	return UpdateTrackableTaskAttribute(pKI, KANBANITEM::GetAttributeID(nAttrib), aNewValues);
}

BOOL CKanbanCtrl::UpdateTrackableTaskAttribute(KANBANITEM* pKI, IUI_ATTRIBUTE nAttrib, const CStringArray& aNewValues)
{
	switch (nAttrib)
	{
	case IUI_ALLOCTO:
	case IUI_CATEGORY:
	case IUI_TAGS:
		if (aNewValues.GetSize() == 0)
		{
			CStringArray aTemp;
			aTemp.Add(_T(""));

			return UpdateTrackableTaskAttribute(pKI, nAttrib, aTemp); // RECURSIVE CALL
		}
		break;

	default:
		ASSERT(0);
		return FALSE;
	}

	return UpdateTrackableTaskAttribute(pKI, KANBANITEM::GetAttributeID(nAttrib), aNewValues);
}

BOOL CKanbanCtrl::UpdateTrackableTaskAttribute(KANBANITEM* pKI, const CString& sAttribID, const CStringArray& aNewValues)
{
	// Check if we need to update listctrls or not
	if (!IsTracking(sAttribID) || (pKI->bParent && !HasOption(KBCF_SHOWPARENTTASKS)))
	{
		pKI->SetTrackedAttributeValues(sAttribID, aNewValues);
		return FALSE; // no effect on list items
	}

	// else
	BOOL bChange = FALSE;
	
	if (!pKI->AttributeValuesMatch(sAttribID, aNewValues))
	{
		CStringArray aCurValues;
		pKI->GetTrackedAttributeValues(sAttribID, aCurValues);
		
		// Remove any list item whose current value is not found in the new values
		int nVal = aCurValues.GetSize();
		
		// Special case: Item needs removing from backlog
		if (nVal == 0)
		{
			aCurValues.Add(_T(""));
			nVal++;
		}

		while (nVal--)
		{
			if (!Misc::Contains(aCurValues[nVal], aNewValues))
			{
				CKanbanColumnCtrl* pCurList = m_aListCtrls.Get(aCurValues[nVal]);
				ASSERT(pCurList);

				if (pCurList)
				{
					VERIFY(pCurList->DeleteTask(pKI->dwTaskID));
					bChange |= (pCurList->GetCount() == 0);
				}

				// Remove from list to speed up later searching
				aCurValues.RemoveAt(nVal);
			}
		}
		
		// Add any new items not in the current list
		nVal = aNewValues.GetSize();
		
		while (nVal--)
		{
			if (!Misc::Contains(aNewValues[nVal], aCurValues))
			{
				CKanbanColumnCtrl* pCurList = m_aListCtrls.Get(aNewValues[nVal]);
				
				if (pCurList)
					pCurList->AddTask(*pKI, FALSE);
				else
					bChange = TRUE; // needs new list ctrl
			}
		}
	
		// update values
		pKI->SetTrackedAttributeValues(sAttribID, aNewValues);
	}
	
	return bChange;
}

BOOL CKanbanCtrl::IsTracking(const CString& sAttribID) const
{
	return (m_sTrackAttribID.CompareNoCase(sAttribID) == 0);
}

BOOL CKanbanCtrl::WantShowColumn(LPCTSTR szValue, const CKanbanItemArrayMap& mapKIArray) const
{
	if (HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return TRUE;

	if (HasOption(KBCF_ALWAYSSHOWBACKLOG) && Misc::IsEmpty(szValue))
		return TRUE;

	// else
	const CKanbanItemArray* pKIArr = mapKIArray.GetMapping(szValue);
		
	return (pKIArr && pKIArr->GetSize());
}

BOOL CKanbanCtrl::WantShowColumn(const CKanbanColumnCtrl* pList) const
{
	if (HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return TRUE;

	if (HasOption(KBCF_ALWAYSSHOWBACKLOG) && pList->IsBacklog())
		return TRUE;

	return (pList->GetCount() > 0);
}

BOOL CKanbanCtrl::DeleteListCtrl(int nList)
{
	if ((nList < 0) || (nList >= m_aListCtrls.GetSize()))
	{
		ASSERT(0);
		return FALSE;
	}

	CKanbanColumnCtrl* pList = m_aListCtrls[nList];
	ASSERT(pList);

	if (pList == m_pSelectedList)
		m_pSelectedList = NULL;

	m_aListCtrls.RemoveAt(nList);

	return TRUE;
}

BOOL CKanbanCtrl::HasNonParentTasks(const CKanbanItemArray* pItems)
{
	ASSERT(pItems);

	int nItem = pItems->GetSize();

	while (nItem--)
	{
		if (!pItems->GetAt(nItem)->bParent)
			return TRUE;
	}

	// else all parents
	return FALSE;
}

int CKanbanCtrl::RemoveOldDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return 0;
	}

	// remove any lists whose values are no longer used
	// or which Optionally have no items 
	const CKanbanValueMap* pGlobals = m_mapAttributeValues.GetMapping(m_sTrackAttribID);
	int nList = m_aListCtrls.GetSize(), nNumRemoved = 0;
	
	while (nList--)
	{
		CKanbanColumnCtrl* pList = m_aListCtrls[nList];
		ASSERT(pList && !pList->HasMultipleValues());
		
		if ((pGlobals == NULL) || !WantShowColumn(pList))
		{
			DeleteListCtrl(nList);
			nNumRemoved++;
		}
		else
		{
			CString sAttribValueID(pList->GetAttributeValueID());
			
			if (!Misc::HasKey(*pGlobals, sAttribValueID) || 
				!WantShowColumn(sAttribValueID, mapKIArray))
			{
				DeleteListCtrl(nList);
				nNumRemoved++;
			}
		}
	}

	return nNumRemoved;
}

int CKanbanCtrl::AddMissingDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return 0;
	}
	
	// Add any new status lists not yet existing
	const CKanbanValueMap* pGlobals = m_mapAttributeValues.GetMapping(m_sTrackAttribID);
	int nNumAdded = 0;

	if (pGlobals)
	{
		POSITION pos = pGlobals->GetStartPosition();
		
		while (pos)
		{
			CString sAttribValueID, sAttribValue;
			pGlobals->GetNextAssoc(pos, sAttribValueID, sAttribValue);
			
			CKanbanColumnCtrl* pList = m_aListCtrls.Get(sAttribValueID);
			
			if ((pList == NULL) && WantShowColumn(sAttribValueID, mapKIArray))
			{
				KANBANCOLUMN colDef;
				
				colDef.sAttribID = m_sTrackAttribID;
				colDef.sTitle = sAttribValue;
				colDef.aAttribValues.Add(sAttribValue);
				//colDef.crBackground = KBCOLORS[m_nNextColor++ % NUM_KBCOLORS];
				
				VERIFY (AddNewListCtrl(colDef) != NULL);
				nNumAdded++;
			}
		}

		ASSERT(!HasOption(KBCF_SHOWEMPTYCOLUMNS) || 
				(m_nTrackAttribute == IUI_CUSTOMATTRIB) ||
				(m_aListCtrls.GetSize() == pGlobals->GetCount()));
	}

	return nNumAdded;
}

void CKanbanCtrl::RebuildDynamicListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingDynamicColumns())
	{
		ASSERT(0);
		return;
	}
	
	BOOL bNeedResize = RemoveOldDynamicListCtrls(mapKIArray);
	bNeedResize |= AddMissingDynamicListCtrls(mapKIArray);

	// If no columns created, create empty Backlog column
	bNeedResize |= CheckAddBacklogListCtrl();
	
	// (Re)sort
	m_aListCtrls.Sort();
}

void CKanbanCtrl::RebuildFixedListCtrls(const CKanbanItemArrayMap& mapKIArray)
{
	if (!UsingFixedColumns())
	{
		ASSERT(0);
		return;
	}

	if (m_aListCtrls.GetSize() == 0) // first time only
	{
		for (int nDef = 0; nDef < m_aColumnDefs.GetSize(); nDef++)
		{
			const KANBANCOLUMN& colDef = m_aColumnDefs[nDef];
			VERIFY(AddNewListCtrl(colDef) != NULL);
		}
	}
}

BOOL CKanbanCtrl::CheckAddBacklogListCtrl()
{
	if (m_aListCtrls.GetSize() == 0) 
	{
		KANBANCOLUMN colDef;
		
		colDef.sAttribID = m_sTrackAttribID;
		colDef.aAttribValues.Add(_T(""));
		
		VERIFY (AddNewListCtrl(colDef) != NULL);
		return TRUE;
	}

	return FALSE;
}

void CKanbanCtrl::RebuildListCtrls(BOOL bRebuildData, BOOL bTaskUpdate)
{
	if (m_sTrackAttribID.IsEmpty())
	{
		ASSERT(m_nTrackAttribute == IUI_NONE);
		return;
	}

	CHoldRedraw gr(*this, NCR_PAINT | NCR_ERASEBKGND);

	DWORD dwSelTaskID = GetSelectedTaskID();
	
	CKanbanItemArrayMap mapKIArray;
	m_data.BuildTempItemMaps(m_sTrackAttribID, mapKIArray);

	if (UsingDynamicColumns())
		RebuildDynamicListCtrls(mapKIArray);
	else
		RebuildFixedListCtrls(mapKIArray);

	// Rebuild the list data for each list (which can be empty)
	if (bRebuildData)
	{
		RebuildListCtrlData(mapKIArray);
	}
	else if (UsingDynamicColumns())
	{
		// If not rebuilding the data (which will remove lists
		// which are empty as consequence of not showing parent
		// tasks) we made need to remove such lists.
		RemoveOldDynamicListCtrls(mapKIArray);
	}

	RebuildHeaderColumns();
	Resize();
		
	// We only need to restore selection if not doing a task update
	// because the app takes care of that
	if (!bTaskUpdate && dwSelTaskID && !SelectTask(dwSelTaskID))
	{
		if (!m_pSelectedList || !Misc::HasT(m_aListCtrls, m_pSelectedList))
		{
			// Find the first list with some items
			m_pSelectedList = m_aListCtrls.GetFirstNonEmpty();

			// No list has items?
			if (!m_pSelectedList)
				m_pSelectedList = m_aListCtrls[0];
		}
	}
}

void CKanbanCtrl::RebuildHeaderColumns()
{
	int nNumVisColumns = GetVisibleListCtrlCount();

	if (!nNumVisColumns)
		return;

	// Remove excess columns
	while (nNumVisColumns < m_header.GetItemCount())
	{
		m_header.DeleteItem(0);
	}

	// Add new columns
	if (m_header.GetItemCount() < nNumVisColumns)
	{
		// Give new columns the average width of old columns
		int nNewColWidth = 1;

		if (m_header.GetItemCount())
			nNewColWidth = m_header.CalcAverageItemWidth();

		while (nNumVisColumns > m_header.GetItemCount())
		{
			m_header.AppendItem(nNewColWidth);
		}
	}

	int nNumColumns = m_aListCtrls.GetSize();

	for (int nCol = 0, nVis = 0; nCol < nNumColumns; nCol++)
	{
		const CKanbanColumnCtrl* pList = m_aListCtrls[nCol];

		if (!WantShowColumn(pList))
			continue;

		CEnString sTitle(pList->ColumnDefinition().sTitle);

		if (sTitle.IsEmpty())
			sTitle.LoadString(IDS_BACKLOG);

		CString sFormat;
		sFormat.Format(_T("%s (%d)"), sTitle, pList->GetCount());

		m_header.SetItemText(nVis, sFormat);
		m_header.SetItemData(nVis, (DWORD)pList);
		nVis++;

		// Allow tracking on all but the last column
		m_header.EnableItemTracking(nCol, (nCol != (nNumColumns - 1)));
	}
}

void CKanbanCtrl::RebuildListCtrlData(const CKanbanItemArrayMap& mapKIArray)
{
	BOOL bShowParents = HasOption(KBCF_SHOWPARENTTASKS);
	int nList = m_aListCtrls.GetSize();
	
	while (nList--)
	{
		CKanbanColumnCtrl* pList = m_aListCtrls[nList];
		ASSERT(pList);
		
		RebuildListContents(pList, mapKIArray, bShowParents);
		
		// The list can still end up empty if parent tasks are 
		// omitted in Dynamic columns so we recheck and delete if required
		if (UsingDynamicColumns())
		{
			if (!bShowParents && !WantShowColumn(pList))
			{
				DeleteListCtrl(nList);
			}
		}
	}
		
	// Lists can still end up empty if there were 
	// only unwanted parents
	CheckAddBacklogListCtrl();

	// Resort
	Sort(m_nSortBy, m_bSortAscending);
}

void CKanbanCtrl::FixupSelectedList()
{
	ASSERT(m_aListCtrls.GetSize());

	// Make sure selected list is valid
	if (!m_pSelectedList || !Misc::HasT(m_aListCtrls, m_pSelectedList))
	{
		// Find the first list with some items
		m_pSelectedList = m_aListCtrls.GetFirstNonEmpty();

		// No list has items?
		if (!m_pSelectedList)
			m_pSelectedList = m_aListCtrls[0];
	}

	FixupListFocus();
}

void CKanbanCtrl::FixupListFocus()
{
	const CWnd* pFocus = GetFocus();

	if (IsWindowVisible() && HasFocus() && m_pSelectedList && (pFocus != m_pSelectedList))
	{
		{
			CAutoFlag af(m_bSettingListFocus, TRUE);

			m_pSelectedList->SetFocus();
			m_pSelectedList->Invalidate(TRUE);
		}

		if (pFocus)
		{
			CKanbanColumnCtrl* pOtherList = m_aListCtrls.Get(*pFocus);

			if (pOtherList)
				pOtherList->ClearSelection();
		}
	}
}

IUI_ATTRIBUTE CKanbanCtrl::GetTrackedAttribute(CString& sCustomAttrib) const
{
	if (m_nTrackAttribute == IUI_CUSTOMATTRIB)
		sCustomAttrib = m_sTrackAttribID;
	else
		sCustomAttrib.Empty();

	return m_nTrackAttribute;
}

BOOL CKanbanCtrl::TrackAttribute(IUI_ATTRIBUTE nAttrib, const CString& sCustomAttribID, 
								 const CKanbanColumnArray& aColumnDefs)
{
	// validate input and check for changes
	BOOL bChange = (nAttrib != m_nTrackAttribute);

	switch (nAttrib)
	{
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_TAGS:
		break;
		
	case IUI_CUSTOMATTRIB:
		if (sCustomAttribID.IsEmpty())
			return FALSE;

		if (!bChange)
			bChange = (sCustomAttribID != m_sTrackAttribID);
		break;

	default:
		return FALSE;
	}

	// Check if only display attributes have changed
	if (!bChange)
	{
		if (UsingFixedColumns())
		{
			if (m_aColumnDefs.MatchesAll(aColumnDefs))
			{
				return TRUE;
			}
			else if (m_aColumnDefs.MatchesAll(aColumnDefs, FALSE))
			{
				int nCol = aColumnDefs.GetSize();
				ASSERT(nCol == m_aListCtrls.GetSize());

				while (nCol--)
				{
					const KANBANCOLUMN& colDef = aColumnDefs[nCol];
					CKanbanColumnCtrl* pList = m_aListCtrls[nCol];
					ASSERT(pList);

					if (pList)
					{
						pList->SetBackgroundColor(colDef.crBackground);
						//pList->SetExcessColor(colDef.crExcess);
						//pList->SetMaximumTaskCount(colDef.nMaxTaskCount);
					}
				}
				return TRUE;
			}
		}
		else if (!aColumnDefs.GetSize()) // not switching to fixed columns
		{
			return TRUE;
		}
	}

	m_aColumnDefs.Copy(aColumnDefs);

	// update state
	m_nTrackAttribute = nAttrib;

	switch (nAttrib)
	{
	case IUI_STATUS:
	case IUI_ALLOCTO:
	case IUI_ALLOCBY:
	case IUI_CATEGORY:
	case IUI_VERSION:
	case IUI_PRIORITY:
	case IUI_RISK:
	case IUI_TAGS:
		m_sTrackAttribID = KANBANITEM::GetAttributeID(nAttrib);
		break;
		
	case IUI_CUSTOMATTRIB:
		m_sTrackAttribID = sCustomAttribID;
		break;
	}

	// delete all lists and start over
	CHoldRedraw gr(*this, NCR_PAINT | NCR_ERASEBKGND);

	m_pSelectedList = NULL;
	m_aListCtrls.RemoveAll();

	RebuildListCtrls(TRUE, TRUE);
	Resize();

	return TRUE;
}

CKanbanColumnCtrl* CKanbanCtrl::AddNewListCtrl(const KANBANCOLUMN& colDef)
{
	CKanbanColumnCtrl* pList = new CKanbanColumnCtrl(m_data, colDef, m_fonts, m_aPriorityColors, m_aDisplayAttrib);
	ASSERT(pList);

	if (pList)
	{
		pList->SetOptions(m_dwOptions);

		if (pList->Create(IDC_LISTCTRL, this))
		{
			m_aListCtrls.Add(pList);
		}
		else
		{
			delete pList;
			pList = NULL;
		}
	}
	
	return pList;
}

BOOL CKanbanCtrl::RebuildListContents(CKanbanColumnCtrl* pList, const CKanbanItemArrayMap& mapKIArray, BOOL bShowParents)
{
	ASSERT(pList && pList->GetSafeHwnd());

	if (!pList || !pList->GetSafeHwnd())
		return FALSE;

	DWORD dwSelID = pList->GetSelectedTaskID();

	pList->SetRedraw(FALSE);
	pList->DeleteAllItems();

	CStringArray aValueIDs;
	int nNumVals = pList->GetAttributeValueIDs(aValueIDs);

	for (int nVal = 0; nVal < nNumVals; nVal++)
	{
		const CKanbanItemArray* pKIArr = mapKIArray.GetMapping(aValueIDs[nVal]);
		
		if (pKIArr)
		{
			ASSERT(pKIArr->GetSize());
			
			for (int nKI = 0; nKI < pKIArr->GetSize(); nKI++)
			{
				const KANBANITEM* pKI = pKIArr->GetAt(nKI);
				ASSERT(pKI);
				
				if (!pKI->bParent || bShowParents)
				{
					BOOL bSelected = (dwSelID == pKI->dwTaskID);

					VERIFY(pList->AddTask(*pKI, bSelected) != NULL);
				}
			}
		}
	}
	
	pList->RefreshItemLineHeights();
	pList->SetRedraw(TRUE);

	return TRUE;
}

void CKanbanCtrl::BuildTaskIDMap(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CDWordSet& mapIDs, BOOL bAndSiblings)
{
	if (hTask == NULL)
		return;
	
	mapIDs.Add(pTasks->GetTaskID(hTask));
	
	// children
	BuildTaskIDMap(pTasks, pTasks->GetFirstTask(hTask), mapIDs, TRUE);
	
	// handle siblings WITHOUT RECURSION
	if (bAndSiblings)
	{
		HTASKITEM hSibling = pTasks->GetNextTask(hTask);
		
		while (hSibling)
		{
			// FALSE == not siblings
			BuildTaskIDMap(pTasks, hSibling, mapIDs, FALSE);
			hSibling = pTasks->GetNextTask(hSibling);
		}
	}
}

void CKanbanCtrl::RemoveDeletedTasks(const ITASKLISTBASE* pTasks)
{
	CDWordSet mapIDs;
	BuildTaskIDMap(pTasks, pTasks->GetFirstTask(NULL), mapIDs, TRUE);

	m_aListCtrls.RemoveDeletedTasks(mapIDs);
	m_data.RemoveDeletedItems(mapIDs);
}

KANBANITEM* CKanbanCtrl::GetKanbanItem(DWORD dwTaskID) const
{
	return m_data.GetItem(dwTaskID);
}

BOOL CKanbanCtrl::HasKanbanItem(DWORD dwTaskID) const
{
	return m_data.HasItem(dwTaskID);
}

CKanbanColumnCtrl* CKanbanCtrl::LocateTask(DWORD dwTaskID, HTREEITEM& hti, BOOL bForward) const
{
	// First try selected list
	if (m_pSelectedList)
	{
		hti = m_pSelectedList->FindTask(dwTaskID);

		if (hti)
			return m_pSelectedList;
	}

	// try any other list in the specified direction
	const CKanbanColumnCtrl* pList = GetNextListCtrl(m_pSelectedList, bForward, TRUE);

	if (!pList)
		return NULL;

	const CKanbanColumnCtrl* pStartList = pList;

	do
	{
		hti = pList->FindTask(dwTaskID);

		if (hti)
			return const_cast<CKanbanColumnCtrl*>(pList);

		// else
		pList = GetNextListCtrl(pList, bForward, TRUE);
	}
	while (pList != pStartList);

	return NULL;
}

void CKanbanCtrl::SetDisplayAttributes(const CKanbanAttributeArray& aAttrib)
{
	if (!Misc::MatchAllT(m_aDisplayAttrib, aAttrib, FALSE))
	{
		m_aDisplayAttrib.Copy(aAttrib);
		m_aListCtrls.OnDisplayAttributeChanged();

		// Update list attribute label visibility
		if (m_aDisplayAttrib.GetSize())
			Resize();
	}
}

int CKanbanCtrl::GetDisplayAttributes(CKanbanAttributeArray& aAttrib) const
{
	aAttrib.Copy(m_aDisplayAttrib);
	return aAttrib.GetSize();
}

void CKanbanCtrl::SetOptions(DWORD dwOptions)
{
	if (dwOptions != m_dwOptions)
	{
		if (Misc::FlagHasChanged(KBCF_SHOWPARENTTASKS, m_dwOptions, dwOptions))
		{
			RebuildListCtrls(TRUE, FALSE);
		}
		else if (Misc::FlagHasChanged(KBCF_SHOWEMPTYCOLUMNS | KBCF_ALWAYSSHOWBACKLOG, m_dwOptions, dwOptions))
		{
			RebuildListCtrls(FALSE, FALSE);
		}

		m_dwOptions = dwOptions;

		m_aListCtrls.SetOptions(dwOptions & ~(KBCF_SHOWPARENTTASKS | KBCF_SHOWEMPTYCOLUMNS | KBCF_ALWAYSSHOWBACKLOG));
	}
}

void CKanbanCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	Resize(cx, cy);
}

BOOL CKanbanCtrl::OnEraseBkgnd(CDC* pDC)
{
	if (m_aListCtrls.GetSize())
	{
		CDialogHelper::ExcludeChild(&m_header, pDC);

		// Clip out the list controls
		m_aListCtrls.Exclude(pDC);
		
		// fill the client with gray to create borders and dividers
		CRect rClient;
		GetClientRect(rClient);
		
		pDC->FillSolidRect(rClient, GetSysColor(COLOR_3DSHADOW));
	}
	
	return TRUE;
}

void CKanbanCtrl::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);

	FixupListFocus();
	ScrollToSelectedTask();
}

int CKanbanCtrl::GetVisibleListCtrlCount() const
{
	if (UsingDynamicColumns() || HasOption(KBCF_SHOWEMPTYCOLUMNS))
		return m_aListCtrls.GetSize();

	// Fixed columns
	BOOL bAlwaysShowBacklog = HasOption(KBCF_ALWAYSSHOWBACKLOG);
	int nList = m_aListCtrls.GetSize(), nNumVis = 0;

	while (nList--)
	{
		const CKanbanColumnCtrl* pList = m_aListCtrls[nList];
		ASSERT(pList);

		if (!pList->GetCount())
		{
			if (!bAlwaysShowBacklog || !pList->IsBacklog())
				continue;
		}

		nNumVis++;
	}

	return nNumVis;
}

void CKanbanCtrl::Resize(int cx, int cy)
{
	int nNumVisibleLists = GetVisibleListCtrlCount();

	if (nNumVisibleLists)
	{
		CDeferWndMove dwm(nNumVisibleLists + 1);
		CRect rAvail(0, 0, cx, cy);

		if (rAvail.IsRectEmpty())
			GetClientRect(rAvail);

		// Create a border
		rAvail.DeflateRect(1, 1);

		ResizeHeader(dwm, rAvail);
		
		CRect rList(rAvail);
		CWnd* pPrev = NULL;
		int nNumLists = m_aListCtrls.GetSize();

		for (int nList = 0, nVis = 0; nList < nNumLists; nList++)
		{
			CKanbanColumnCtrl* pList = m_aListCtrls[nList];
			ASSERT(pList && pList->GetSafeHwnd());
			
			// If we find an empty column, it can only be with 
			// Fixed columns because we only hide columns rather
			// than delete them
			if (UsingFixedColumns())
			{
				if (!WantShowColumn(pList))
				{
					pList->ShowWindow(SW_HIDE);
					pList->EnableWindow(FALSE);
					continue;
				}

				// else
				pList->ShowWindow(SW_SHOW);
				pList->EnableWindow(TRUE);
			}

			rList.right = (rList.left + m_header.GetItemWidth(nVis));
			dwm.MoveWindow(pList, rList.left, rList.top, rList.Width() - 1, rList.Height(), FALSE);

			// Also update tab order as we go
			pList->SetWindowPos(pPrev, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOACTIVATE);

			// Check whether the lists are wide enough to show attribute labels
			KBC_ATTRIBLABELS nLabelVis = GetListAttributeLabelVisibility(nList, rList.Width());
			pList->SetAttributeLabelVisibility(nLabelVis);

			pPrev = pList;
			rList.left = rList.right;
			nVis++;
		}
	}
}

void CKanbanCtrl::ResizeHeader(CDeferWndMove& dwm, CRect& rAvail)
{
	CAutoFlag af(m_bResizingHeader, TRUE);

	ASSERT(m_header.GetSafeHwnd());

	int nNumCols = m_header.GetItemCount();
	ASSERT(nNumCols == GetVisibleListCtrlCount());

	CRect rNewHeader(rAvail);
	rNewHeader.bottom = (rNewHeader.top + HEADER_HEIGHT);

	dwm.MoveWindow(&m_header, rNewHeader, FALSE);
		
	int nTotalColWidth = m_header.CalcTotalItemWidth();

	for (int nCol = 0, nColStart = 0; nCol < nNumCols; nCol++)
	{
		if (nCol < (nNumCols - 1))
		{
			int nCurWidth = m_header.GetItemWidth(nCol);
			int nNewWidth = MulDiv(nCurWidth, rNewHeader.Width(), nTotalColWidth);

			m_header.SetItemWidth(nCol, nNewWidth);
			nColStart += nNewWidth;
		}
		else
		{
			int nNewWidth = (rNewHeader.Width() - nColStart);
			m_header.SetItemWidth(nCol, nNewWidth + 1); // +1 hides the divider
		}
	}

	rAvail.top = rNewHeader.bottom;
}

float CKanbanCtrl::GetAverageListCharWidth()
{
	return m_aListCtrls.GetAverageCharWidth();
}

BOOL CKanbanCtrl::CanFitAttributeLabels(int nAvailWidth, float fAveCharWidth, KBC_ATTRIBLABELS nLabelVis) const
{
	switch (nLabelVis)
	{
	case KBCAL_NONE:
		return TRUE;

	case KBCAL_LONG:
	case KBCAL_SHORT:
		{
			int nAtt = m_aDisplayAttrib.GetSize();
			CUIntArray aLabelLen;

			aLabelLen.SetSize(nAtt);

			while (nAtt--)
			{
				IUI_ATTRIBUTE nAttribID = m_aDisplayAttrib[nAtt];
				CString sLabel = CKanbanColumnCtrl::FormatAttribute(nAttribID, _T(""), nLabelVis);

				aLabelLen[nAtt] = sLabel.GetLength();

				if ((int)(aLabelLen[nAtt] * fAveCharWidth) > nAvailWidth)
					return FALSE;
			}

			// Look for the first 'Label: Value' item which exceeds the list width
			POSITION pos = m_data.GetStartPosition();

			while (pos)
			{
				KANBANITEM* pKI = NULL;
				DWORD dwTaskID = 0;

				m_data.GetNextAssoc(pos, dwTaskID, pKI);

				int nAtt = m_aDisplayAttrib.GetSize();

				while (nAtt--)
				{
					IUI_ATTRIBUTE nAttribID = m_aDisplayAttrib[nAtt];

					// Exclude 'File Link' and 'Parent' because these will 
					// almost always push things over the limit
					// Exclude 'flag' because that is rendered as an icon
					switch (nAttribID)
					{
					case IUI_FILEREF:
					case IUI_PARENT:
					case IUI_FLAG:
						continue;
					}

					// else
					int nValueLen = pKI->GetAttributeDisplayValue(nAttribID).GetLength();

					if ((int)((aLabelLen[nAtt] + nValueLen) * fAveCharWidth) > nAvailWidth)
						return FALSE;
				}
			}

			// else
			return TRUE;
		}
		break;
	}

	// all else
	ASSERT(0);
	return FALSE;
}

KBC_ATTRIBLABELS CKanbanCtrl::GetListAttributeLabelVisibility(int nList, int nListWidth)
{
	if (!m_aDisplayAttrib.GetSize() || !m_aListCtrls.GetSize())
		return KBCAL_NONE;

	// Calculate the available width for attributes
	int nAvailWidth = m_aListCtrls[nList]->CalcAvailableAttributeWidth(nListWidth);

	// Calculate the fixed attribute label lengths and check if any
	// of them exceed the list width
	float fAveCharWidth = GetAverageListCharWidth();
	KBC_ATTRIBLABELS nLabelVis[2] = { KBCAL_LONG, KBCAL_SHORT };

	for (int nPass = 0; nPass < 2; nPass++)
	{
		if (CanFitAttributeLabels(nAvailWidth, fAveCharWidth, nLabelVis[nPass]))
			return nLabelVis[nPass];
	}

	return KBCAL_NONE;
}

void CKanbanCtrl::Sort(IUI_ATTRIBUTE nBy, BOOL bAscending)
{
	// if the sort attribute equals the track attribute then
	// tasks are already sorted into separate columns so we  
	// sort by title instead
	if ((nBy != IUI_NONE) && (nBy == m_nTrackAttribute))
		nBy = IUI_TASKNAME;
	
	m_nSortBy = nBy;

	if ((m_nSortBy != IUI_NONE) || HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS))
	{
		ASSERT((m_nSortBy == IUI_NONE) || (bAscending != -1));
		m_bSortAscending = bAscending;

		// do the sort
 		CHoldRedraw hr(*this);

		m_aListCtrls.SortItems(m_nSortBy, m_bSortAscending);
	}
}

void CKanbanCtrl::SetReadOnly(bool bReadOnly) 
{ 
	m_bReadOnly = bReadOnly; 
}

BOOL CKanbanCtrl::GetLabelEditRect(LPRECT pEdit)
{
	if (!m_pSelectedList || !m_pSelectedList->GetLabelEditRect(pEdit))
	{
		ASSERT(0);
		return FALSE;
	}

	// else convert from list to 'our' coords
	m_pSelectedList->ClientToScreen(pEdit);
	ScreenToClient(pEdit);

	return TRUE;
}

void CKanbanCtrl::SetPriorityColors(const CDWordArray& aColors)
{
	if (!Misc::MatchAll(m_aPriorityColors, aColors))
	{
		m_aPriorityColors.Copy(aColors);

		// Redraw the lists if coloring by priority
		if (GetSafeHwnd() && HasOption(KBCF_COLORBARBYPRIORITY))
			m_aListCtrls.Redraw(FALSE);
	}
}

void CKanbanCtrl::ScrollToSelectedTask()
{
	CKanbanColumnCtrl* pList = GetSelListCtrl();

	if (pList)
		pList->ScrollToSelection();
}

bool CKanbanCtrl::PrepareNewTask(ITaskList* pTask) const
{
	ITASKLISTBASE* pTasks = GetITLInterface<ITASKLISTBASE>(pTask, IID_TASKLISTBASE);

	if (pTasks == NULL)
	{
		ASSERT(0);
		return false;
	}

	HTASKITEM hNewTask = pTasks->GetFirstTask();
	ASSERT(hNewTask);

	const CKanbanColumnCtrl* pList = GetSelListCtrl();
	CString sValue;

	CRect rListCtrl;
	pList->GetWindowRect(rListCtrl);

	if (!GetListCtrlAttributeValue(pList, rListCtrl.CenterPoint(), sValue))
		return false;

	switch (m_nTrackAttribute)
	{
	case IUI_STATUS:
		pTasks->SetTaskStatus(hNewTask, sValue);
		break;

	case IUI_ALLOCTO:
		pTasks->AddTaskAllocatedTo(hNewTask, sValue);
		break;

	case IUI_ALLOCBY:
		pTasks->SetTaskAllocatedBy(hNewTask, sValue);
		break;

	case IUI_CATEGORY:
		pTasks->AddTaskCategory(hNewTask, sValue);
		break;

	case IUI_PRIORITY:
		pTasks->SetTaskPriority(hNewTask, _ttoi(sValue));
		break;

	case IUI_RISK:
		pTasks->SetTaskRisk(hNewTask, _ttoi(sValue));
		break;

	case IUI_VERSION:
		pTasks->SetTaskVersion(hNewTask, sValue);
		break;

	case IUI_TAGS:
		pTasks->AddTaskTag(hNewTask, sValue);
		break;

	case IUI_CUSTOMATTRIB:
		ASSERT(!m_sTrackAttribID.IsEmpty());
		pTasks->SetTaskCustomAttributeData(hNewTask, m_sTrackAttribID, sValue);
		break;
	}

	return true;
}

DWORD CKanbanCtrl::HitTestTask(const CPoint& ptScreen) const
{
	return m_aListCtrls.HitTestTask(ptScreen);
}

DWORD CKanbanCtrl::GetNextTask(DWORD dwTaskID, IUI_APPCOMMAND nCmd) const
{
	BOOL bForward = ((nCmd == IUI_GETPREVTASK) || (nCmd == IUI_GETPREVTOPLEVELTASK));

	HTREEITEM hti = NULL;
	const CKanbanColumnCtrl* pList = LocateTask(dwTaskID, hti, bForward);
	
	if (!pList || (UsingFixedColumns() && !pList->IsWindowVisible()))
	{
		return 0L;
	}
	else if (hti == NULL)
	{
		ASSERT(0);
		return 0L;
	}

	switch (nCmd)
	{
	case IUI_GETNEXTTASK:
		hti = pList->GetNextSiblingItem(hti);
		break;

	case IUI_GETPREVTASK:
		hti = pList->GetPrevSiblingItem(hti);
		break;

	case IUI_GETNEXTTOPLEVELTASK:
		pList = GetNextListCtrl(pList, TRUE, TRUE);
			
		if (pList)
			hti = pList->TCH().GetFirstItem();
		break;

	case IUI_GETPREVTOPLEVELTASK:
		pList = GetNextListCtrl(pList, FALSE, TRUE);
			
		if (pList)
			hti = pList->TCH().GetFirstItem();
		break;

	default:
		ASSERT(0);
	}

	return (hti ? pList->GetTaskID(hti) : 0);
}

const CKanbanColumnCtrl* CKanbanCtrl::GetNextListCtrl(const CKanbanColumnCtrl* pList, BOOL bNext, BOOL bExcludeEmpty) const
{
	return m_aListCtrls.GetNext(pList, bNext, bExcludeEmpty, UsingFixedColumns());
}

CKanbanColumnCtrl* CKanbanCtrl::GetNextListCtrl(const CKanbanColumnCtrl* pList, BOOL bNext, BOOL bExcludeEmpty)
{
	return m_aListCtrls.GetNext(pList, bNext, bExcludeEmpty, UsingFixedColumns());
}

BOOL CKanbanCtrl::IsDragging() const
{
	return (!m_bReadOnly && (::GetCapture() == *this));
}

BOOL CKanbanCtrl::NotifyParentAttibuteChange(DWORD dwTaskID)
{
	ASSERT(!m_bReadOnly);
	ASSERT(dwTaskID);

	return GetParent()->SendMessage(WM_KBC_VALUECHANGE, (WPARAM)GetSafeHwnd(), dwTaskID);
}

void CKanbanCtrl::NotifyParentSelectionChange()
{
	ASSERT(!m_bSelectTasks);

	GetParent()->SendMessage(WM_KBC_SELECTIONCHANGE, GetSelectedTaskID(), 0);
}

// external version
BOOL CKanbanCtrl::CancelOperation()
{
	if (IsDragging())
	{
		ReleaseCapture();
		m_aListCtrls.SetDropTarget(NULL);

		return TRUE;
	}
	
	// else 
	return FALSE;
}

BOOL CKanbanCtrl::SelectListCtrl(CKanbanColumnCtrl* pList, BOOL bNotifyParent)
{
	if (pList)
	{
		if (pList == m_pSelectedList)
		{
			// Make sure header is refreshed
			m_aListCtrls.SetSelectedList(m_pSelectedList);
			return TRUE;
		}

		CKanbanColumnCtrl* pPrevSelList = m_pSelectedList;
		m_pSelectedList = pList;

		FixupListFocus();

		if (pList->GetCount() > 0)
		{
			m_aListCtrls.SetSelectedList(m_pSelectedList);

			if (m_pSelectedList->GetSelectedItem())
				m_pSelectedList->ScrollToSelection();

			if (bNotifyParent)
				NotifyParentSelectionChange();
		}
		else
		{
			pPrevSelList->SetSelected(FALSE);
			m_pSelectedList->SetSelected(TRUE);
		}

		m_header.Invalidate(TRUE);

		return TRUE;
	}

	return FALSE;
}

BOOL CKanbanCtrl::IsSelectedListCtrl(HWND hWnd) const
{
	return (m_pSelectedList && (m_pSelectedList->GetSafeHwnd() == hWnd));
}

void CKanbanCtrl::OnListItemSelChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	// only interested in selection changes caused by the user
	LPNMTREEVIEW pNMTV = (LPNMTREEVIEW)pNMHDR;
	UINT nAction = pNMTV->action;

	if (!m_bSettingListFocus && !m_bSelectTasks && !IsDragging() &&
		(nAction & (TVC_BYMOUSE | TVC_BYKEYBOARD)))
	{
		CKanbanColumnCtrl* pList = m_aListCtrls.Get(pNMHDR->hwndFrom);

		if (pList && (pList != m_pSelectedList))
		{
			CAutoFlag af(m_bSelectTasks, TRUE);
			SelectListCtrl(pList, FALSE);
		}

		NotifyParentSelectionChange();
	}
#ifdef _DEBUG
	else
	{
		int breakpoint = 0;
	}
#endif
}

void CKanbanCtrl::OnListEditLabel(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE; // cancel our edit

	NMTVDISPINFO* pNMTV = (NMTVDISPINFO*)pNMHDR;
	ASSERT(pNMTV->item.lParam);

	GetParent()->SendMessage(WM_KBC_EDITTASKTITLE, pNMTV->item.lParam);
}

void CKanbanCtrl::OnHeaderItemChanging(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bResizingHeader || m_bSavingToImage)
		return;

	NMHEADER* pHDN = (NMHEADER*)pNMHDR;

	if ((pHDN->iButton == 0) && (pHDN->pitem->mask & HDI_WIDTH))
	{
		ASSERT(pHDN->iItem < (m_header.GetItemCount() - 1));

		// prevent 'this' or 'next' columns becoming too small
		int nThisWidth = m_header.GetItemWidth(pHDN->iItem);
		int nNextWidth = m_header.GetItemWidth(pHDN->iItem + 1);

		pHDN->pitem->cxy = max(MIN_COL_WIDTH, pHDN->pitem->cxy);
		pHDN->pitem->cxy = min(pHDN->pitem->cxy, (nThisWidth + nNextWidth - MIN_COL_WIDTH));
		
		// Resize 'next' column
		nNextWidth = (nThisWidth + nNextWidth - pHDN->pitem->cxy);

		CAutoFlag af(m_bResizingHeader, TRUE);
		m_header.SetItemWidth(pHDN->iItem + 1, nNextWidth);
		
		// Resize corresponding listctrls
		CKanbanColumnCtrl* pThisList = m_aListCtrls[pHDN->iItem];
		CKanbanColumnCtrl* pNextList = m_aListCtrls[pHDN->iItem + 1];

		CRect rThisList = CDialogHelper::GetChildRect(pThisList);
		rThisList.right = (rThisList.left + pHDN->pitem->cxy - 1);

		CRect rNextList = CDialogHelper::GetChildRect(m_aListCtrls[pHDN->iItem + 1]);
		rNextList.left = (rThisList.right + 1);

		pThisList->MoveWindow(rThisList, FALSE);
		pNextList->MoveWindow(rNextList, FALSE);

		pThisList->Invalidate(FALSE);
		pNextList->Invalidate(FALSE);

		// Redraw the vertical divider
		CRect rDivider(rThisList);
		rDivider.left = rDivider.right;
		rDivider.right++;

		InvalidateRect(rDivider, TRUE);
	}
}

void CKanbanCtrl::OnHeaderCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMCUSTOMDRAW* pNMCD = (NMCUSTOMDRAW*)pNMHDR;
	*pResult = CDRF_DODEFAULT;

	HWND hwndHdr = pNMCD->hdr.hwndFrom;
	ASSERT(hwndHdr == m_header);
	
	switch (pNMCD->dwDrawStage)
	{
	case CDDS_PREPAINT:
		// Handle RTL text column headers and selected column
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
		
	case CDDS_ITEMPREPAINT:
		if (GraphicsMisc::GetRTLDrawTextFlags(hwndHdr) == DT_RTLREADING)
		{
			*pResult = CDRF_NOTIFYPOSTPAINT;
		}
		else if (!m_bSavingToImage && m_pSelectedList)
		{
			// Show the text of the selected column in bold
			if (pNMCD->lItemlParam == (LPARAM)m_pSelectedList)
				::SelectObject(pNMCD->hdc, m_fonts.GetHFont(GMFS_BOLD));
			else
				::SelectObject(pNMCD->hdc, m_fonts.GetHFont());
			
			*pResult = CDRF_NEWFONT;
		}
		break;
		
	case CDDS_ITEMPOSTPAINT:
		{
			ASSERT(GraphicsMisc::GetRTLDrawTextFlags(hwndHdr) == DT_RTLREADING);

			CRect rItem(pNMCD->rc);
			rItem.DeflateRect(3, 0);

			CDC* pDC = CDC::FromHandle(pNMCD->hdc);
			pDC->SetBkMode(TRANSPARENT);

			// Show the text of the selected column in bold
			HGDIOBJ hPrev = NULL;

			if (!m_bSavingToImage)
			{
				if (pNMCD->lItemlParam == (LPARAM)m_pSelectedList)
					hPrev = pDC->SelectObject(m_fonts.GetHFont(GMFS_BOLD));
				else
					hPrev = pDC->SelectObject(m_fonts.GetHFont());
			}
			
			UINT nFlags = (DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | GraphicsMisc::GetRTLDrawTextFlags(hwndHdr));
			pDC->DrawText(m_header.GetItemText(pNMCD->dwItemSpec), rItem, nFlags);

			if (!m_bSavingToImage)
				pDC->SelectObject(hPrev);
			
			*pResult = CDRF_SKIPDEFAULT;
		}
		break;
	}
}

void CKanbanCtrl::OnBeginDragListItem(NMHDR* pNMHDR, LRESULT* pResult)
{
	ReleaseCapture();

	if (!m_bReadOnly && !IsDragging())
	{
		ASSERT(pNMHDR->idFrom == IDC_LISTCTRL);

		if (Misc::IsKeyPressed(VK_LBUTTON))
		{
			NMTREEVIEW* pNMTV = (NMTREEVIEW*)pNMHDR;
			ASSERT(pNMTV->itemNew.hItem);
		
			CKanbanColumnCtrl* pList = (CKanbanColumnCtrl*)CWnd::FromHandle(pNMHDR->hwndFrom);

			if (!pList->SelectionHasLockedTasks())
			{
				DWORD dwDragID = pNMTV->itemNew.lParam;

				if (dwDragID)
				{
					// If the 'drag-from' list is not currently selected
					// we select it and then reset the selection to the
					// items we have just copied
					if (pList != m_pSelectedList)
					{
						VERIFY(pList->SelectTask(dwDragID));
						SelectListCtrl(pList);

					}

					SetCapture();
					TRACE(_T("CKanbanCtrlEx::OnBeginDragListItem(start drag)\n"));
				}
			}
		}
		else
		{
			// Mouse button already released
			TRACE(_T("CKanbanCtrlEx::OnBeginDragListItem(cancel drag)\n"));
		}
	}
	
	*pResult = 0;
}

BOOL CKanbanCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	CPoint ptCursor(GetMessagePos());
	DWORD dwTaskID = HitTestTask(ptCursor);

	if (m_data.IsLocked(dwTaskID))
		return GraphicsMisc::SetAppCursor(_T("Locked"), _T("Resources\\Cursors"));

	// else
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CKanbanCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	TRACE(_T("CKanbanCtrlEx::OnLButtonUp()\n"));

	if (IsDragging())
	{
		TRACE(_T("CKanbanCtrlEx::OnLButtonUp(end drag)\n"));

		// get the list under the mouse
		ClientToScreen(&point);

		CKanbanColumnCtrl* pDestList = m_aListCtrls.HitTest(point);
		CKanbanColumnCtrl* pSrcList = m_pSelectedList;

		if (CanDrag(pSrcList, pDestList))
		{
			CString sDestAttribValue;
			
			if (GetListCtrlAttributeValue(pDestList, point, sDestAttribValue))
			{
				DWORD dwDragID = pSrcList->GetSelectedTaskID();
				ASSERT(dwDragID);

				BOOL bChange = EndDragItem(pSrcList, dwDragID, pDestList, sDestAttribValue);

				if (!WantShowColumn(pSrcList) && UsingDynamicColumns())
				{
					int nList = Misc::FindT(m_aListCtrls, pSrcList);
					ASSERT(nList != -1);

					m_aListCtrls.RemoveAt(nList);
				}

				Resize();

				if (bChange)
				{
					// Resort before fixing up selection
					if ((m_nSortBy != IUI_NONE) || HasOption(KBCF_SORTSUBTASTASKSBELOWPARENTS))
						pDestList->Sort(m_nSortBy, m_bSortAscending);

					SelectListCtrl(pDestList, FALSE);
					SelectTask(dwDragID); 

					NotifyParentSelectionChange();
					NotifyParentAttibuteChange(dwDragID);
				}
			}
		}

		// always
		m_aListCtrls.SetDropTarget(NULL);
		ReleaseCapture();
	}

	CWnd::OnLButtonUp(nFlags, point);
}

BOOL CKanbanCtrl::CanDrag(const CKanbanColumnCtrl* pSrcList, const CKanbanColumnCtrl* pDestList) const
{
	// Can only copy MULTI-VALUE attributes
	if (Misc::ModKeysArePressed(MKS_CTRL) && !IsTrackedAttributeMultiValue())
		return FALSE;

	return CKanbanColumnCtrl::CanDrag(pSrcList, pDestList);
}

BOOL CKanbanCtrl::EndDragItem(CKanbanColumnCtrl* pSrcList, DWORD dwTaskID, 
								CKanbanColumnCtrl* pDestList, const CString& sDestAttribValue)
{
	ASSERT(CanDrag(pSrcList, pDestList));
	ASSERT(pSrcList->FindTask(dwTaskID) != NULL);

	KANBANITEM* pKI = GetKanbanItem(dwTaskID);

	if (!pKI)
	{
		ASSERT(0);
		return FALSE;
	}

	BOOL bSrcIsBacklog = pSrcList->IsBacklog();
	BOOL bDestIsBacklog = pDestList->IsBacklog();
	BOOL bCopy = (!bSrcIsBacklog && 
					Misc::ModKeysArePressed(MKS_CTRL) &&
					IsTrackedAttributeMultiValue());

	// Remove from the source list(s) if moving
	if (bSrcIsBacklog)
	{
		VERIFY(pSrcList->DeleteTask(dwTaskID));
	}
	else if (!bCopy) // move
	{
		// Remove all values
		pKI->RemoveAllTrackedAttributeValues(m_sTrackAttribID);

		// Remove from all src lists
		m_aListCtrls.DeleteTaskFromOthers(dwTaskID, pDestList);
	}
	else if (bDestIsBacklog) // and 'copy'
	{
		// Just remove the source list's value(s)
		CStringArray aSrcValues;
		int nVal = pSrcList->GetAttributeValues(aSrcValues);

		while (nVal--)
			pKI->RemoveTrackedAttributeValue(m_sTrackAttribID, aSrcValues[nVal]);

		VERIFY(pSrcList->DeleteTask(dwTaskID));
	}

	// Append to the destination list
	if (bDestIsBacklog)
	{
		if (!pKI->HasTrackedAttributeValues(m_sTrackAttribID))
			pDestList->AddTask(*pKI, TRUE);
	}
	else
	{
		pKI->AddTrackedAttributeValue(m_sTrackAttribID, sDestAttribValue);

		if (pDestList->FindTask(dwTaskID) == NULL)
			pDestList->AddTask(*pKI, TRUE);
	}

	return TRUE;
}

BOOL CKanbanCtrl::GetListCtrlAttributeValue(const CKanbanColumnCtrl* pDestList, const CPoint& ptScreen, CString& sValue) const
{
	CStringArray aListValues;
	int nNumValues = pDestList->GetAttributeValues(aListValues);

	switch (nNumValues)
	{
	case 0: // Backlog
		sValue.Empty();
		return TRUE;
		
	case 1:
		sValue = aListValues[0];
		return TRUE;
	}

	// List has multiple values -> show popup menu
	CMenu menu;
	VERIFY (menu.CreatePopupMenu());

	for (int nVal = 0; nVal < nNumValues; nVal++)
	{
		menu.AppendMenu(MF_STRING, (nVal + 1), aListValues[nVal]);
	}

	UINT nValID = menu.TrackPopupMenu((TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD), 
										ptScreen.x, ptScreen.y, CWnd::FromHandle(*pDestList));

	if (nValID > 0)
	{
		sValue = aListValues[nValID - 1];
		return TRUE;
	}

	// user cancelled
	return FALSE;
}

void CKanbanCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (IsDragging())
	{
		// get the list and item under the mouse
		ClientToScreen(&point);

		const CKanbanColumnCtrl* pDestList = m_aListCtrls.HitTest(point);
		BOOL bValidDest = CanDrag(m_pSelectedList, pDestList);

		if (bValidDest)
		{
			BOOL bCopy = Misc::ModKeysArePressed(MKS_CTRL);
			GraphicsMisc::SetDragDropCursor(bCopy ? GMOC_COPY : GMOC_MOVE);
		}
		else
		{
			GraphicsMisc::SetDragDropCursor(GMOC_NO);
		}

		m_aListCtrls.SetDropTarget(bValidDest ? pDestList : NULL);
	}
	
	CWnd::OnMouseMove(nFlags, point);
}

BOOL CKanbanCtrl::SaveToImage(CBitmap& bmImage)
{
	CAutoFlag af(m_bSavingToImage, TRUE);
	CEnBitmap bmLists;

	if (m_aListCtrls.SaveToImage(bmLists))
	{
		// Resize header and column widths to suit
		CIntArray aColWidths;
		m_header.GetItemWidths(aColWidths);

		int nTotalListWidth = bmLists.GetSize().cx;
		int nItem = m_header.GetItemCount();

		int nReqColWidth = (nTotalListWidth / nItem);

		while (nItem--)
			m_header.SetItemWidth(nItem, nReqColWidth);

		CRect rHeader = CDialogHelper::GetChildRect(&m_header);
		CDialogHelper::ResizeChild(&m_header, (nTotalListWidth - rHeader.Width()), 0);

		CEnBitmap bmHeader;

		if (CCopyHeaderCtrlContents(m_header).DoCopy(bmHeader))
		{
			// Restore widths
			m_header.SetItemWidths(aColWidths);

			// Create one bitmap to fit both
			CDC dcImage, dcParts;
			CClientDC dc(this);

			if (dcImage.CreateCompatibleDC(&dc) && dcParts.CreateCompatibleDC(&dc))
			{
				// Create the image big enough to fit the tasks and columns side-by-side
				CSize sizeLists = bmLists.GetSize();
				CSize sizeHeader = bmHeader.GetSize();

				CSize sizeImage;

				sizeImage.cx = max(sizeHeader.cx, sizeLists.cx);
				sizeImage.cy = (sizeHeader.cy + sizeLists.cy);

				if (bmImage.CreateCompatibleBitmap(&dc, sizeImage.cx, sizeImage.cy))
				{
					CBitmap* pOldImage = dcImage.SelectObject(&bmImage);

					CBitmap* pOldPart = dcParts.SelectObject(&bmHeader);
					dcImage.BitBlt(0, 0, sizeHeader.cx, sizeHeader.cy, &dcParts, 0, 0, SRCCOPY);

					dcParts.SelectObject(&bmLists);
					dcImage.BitBlt(0, sizeHeader.cy, sizeLists.cx, sizeLists.cy, &dcParts, 0, 0, SRCCOPY);

					dcParts.SelectObject(pOldPart);
					dcImage.SelectObject(pOldImage);
				}
			}
		}

		m_header.MoveWindow(rHeader);
	}

	ScrollToSelectedTask();

	return (bmImage.GetSafeHandle() != NULL);
}

BOOL CKanbanCtrl::CanSaveToImage() const
{
	return m_aListCtrls.CanSaveToImage();
}

LRESULT CKanbanCtrl::OnSetFont(WPARAM wp, LPARAM lp)
{
	m_fonts.Initialise((HFONT)wp, FALSE);
	m_aListCtrls.SetFont((HFONT)wp);
	m_header.SendMessage(WM_SETFONT, wp, lp);

	return 0L;
}

LRESULT CKanbanCtrl::OnListToggleTaskDone(WPARAM /*wp*/, LPARAM lp)
{
	ASSERT(!m_bReadOnly);
	ASSERT(lp);

	DWORD dwTaskID = lp;
	const KANBANITEM* pKI = m_data.GetItem(dwTaskID);

	if (pKI)
	{
		LRESULT lr = GetParent()->SendMessage(WM_KBC_EDITTASKDONE, dwTaskID, !pKI->IsDone(FALSE));

		if (lr && m_data.HasItem(dwTaskID))
			PostMessage(WM_KCM_SELECTTASK, 0, dwTaskID);

		return lr;
	}

	// else
	ASSERT(0);
	return 0L;
}

LRESULT CKanbanCtrl::OnListEditTaskIcon(WPARAM /*wp*/, LPARAM lp)
{
	ASSERT(!m_bReadOnly);
	ASSERT(lp);

	return GetParent()->SendMessage(WM_KBC_EDITTASKICON, (WPARAM)lp);
}

LRESULT CKanbanCtrl::OnListToggleTaskFlag(WPARAM /*wp*/, LPARAM lp)
{
	ASSERT(!m_bReadOnly);
	ASSERT(lp);

	DWORD dwTaskID = lp;
	const KANBANITEM* pKI = m_data.GetItem(dwTaskID);

	if (pKI)
	{
		LRESULT lr = GetParent()->SendMessage(WM_KBC_EDITTASKFLAG, dwTaskID, !pKI->bFlag);

		if (lr && m_data.HasItem(dwTaskID))
		{
			KANBANITEM* pKI = m_data.GetItem(dwTaskID);
			ASSERT(pKI);

			pKI->bFlag = !pKI->bFlag;

			if (m_pSelectedList)
				m_pSelectedList->Invalidate();

			PostMessage(WM_KCM_SELECTTASK, 0, dwTaskID);
		}

		return lr;
	}

	// else
	ASSERT(0);
	return 0L;
}

LRESULT CKanbanCtrl::OnSelectTask(WPARAM /*wp*/, LPARAM lp)
{
	return SelectTask(lp);
}

LRESULT CKanbanCtrl::OnListGetTaskIcon(WPARAM wp, LPARAM lp)
{
	return GetParent()->SendMessage(WM_KBC_GETTASKICON, wp, lp);
}

void CKanbanCtrl::OnListSetFocus(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	// Reverse focus changes outside of our own doing
	if (!m_bSettingListFocus)
	{
		FixupListFocus();
	}
}
