// Fi M_BlISlteredToDoCtrl.cpp: implementation of the CFilteredToDoCtrl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FilteredToDoCtrl.h"
#include "todoitem.h"
#include "resource.h"
#include "tdcstatic.h"
#include "tdcmsg.h"
#include "TDCSearchParamHelper.h"
#include "taskclipboard.h"

#include "..\shared\holdredraw.h"
#include "..\shared\datehelper.h"
#include "..\shared\enstring.h"
#include "..\shared\deferwndmove.h"
#include "..\shared\autoflag.h"
#include "..\shared\holdredraw.h"
#include "..\shared\osversion.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\savefocus.h"
#include "..\shared\filemisc.h"
#include "..\shared\ScopedTimer.h"

#include "..\Interfaces\Preferences.h"
#include "..\Interfaces\IUIExtension.h"

#include <math.h>

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

#ifndef LVS_EX_LABELTIP
#define LVS_EX_LABELTIP     0x00004000
#endif

//////////////////////////////////////////////////////////////////////

const UINT SORTWIDTH = 10;

#ifdef _DEBUG
const UINT ONE_MINUTE = 10000;
#else
const UINT ONE_MINUTE = 60000;
#endif

const UINT TEN_MINUTES = (ONE_MINUTE * 10);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFilteredToDoCtrl::CFilteredToDoCtrl(CUIExtensionMgr& mgrUIExt, 
									 CTDLContentMgr& mgrContent, 
									 CShortcutManager& mgrShortcuts,
									 const CONTENTFORMAT& cfDefault, 
									 const TDCCOLEDITFILTERVISIBILITY& visDefault) 
	:
	CTabbedToDoCtrl(mgrUIExt, mgrContent, mgrShortcuts, cfDefault, visDefault),
	m_visColEditFilter(visDefault),
	m_bIgnoreListRebuild(FALSE),
	m_bIgnoreExtensionUpdate(FALSE)
{
}

CFilteredToDoCtrl::~CFilteredToDoCtrl()
{

}

BEGIN_MESSAGE_MAP(CFilteredToDoCtrl, CTabbedToDoCtrl)
//{{AFX_MSG_MAP(CFilteredToDoCtrl)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_CBN_EDITCHANGE(IDC_DUETIME, OnEditChangeDueTime)
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////

BOOL CFilteredToDoCtrl::OnInitDialog()
{
	CTabbedToDoCtrl::OnInitDialog();

	return FALSE;
}

BOOL CFilteredToDoCtrl::SelectTask(DWORD dwTaskID, BOOL bTrue)
{	
	if (CTabbedToDoCtrl::SelectTask(dwTaskID, bTrue))
		return TRUE;
	
	// If the task is filtered out we toggle the filter and try again
	if (HasAnyFilter() && HasTask(dwTaskID))
	{
		ToggleFilter(); // show all tasks
		
		if (CTabbedToDoCtrl::SelectTask(dwTaskID, bTrue))
			return TRUE;

		// else
		ASSERT(0);
		ToggleFilter(); // restore filter
	}
	
	return FALSE;
}

BOOL CFilteredToDoCtrl::SelectTask(CString sPart, TDC_SELECTTASK nSelect)
{
	return CTabbedToDoCtrl::SelectTask(sPart, nSelect); 
}

BOOL CFilteredToDoCtrl::LoadTasks(const CTaskFile& tasks)
{
	// handle reloading of tasklist with a filter present
	if (GetTaskCount() && m_filter.HasAnyFilter())
	{
		SaveSettings();
	}

	BOOL bViewWasSet = IsViewSet();

	if (!CTabbedToDoCtrl::LoadTasks(tasks))
		return FALSE;

	FTC_VIEW nView = GetTaskView();

	// save visible state
	BOOL bHidden = !IsWindowVisible();

	// reload last view
	if (!bViewWasSet)
	{
		LoadSettings();

		// always refresh the tree filter because all other
		// views depend on it
		if (HasAnyFilter())
			RefreshTreeFilter(); // always

		// handle other views
		switch (nView)
		{
		case FTCV_TASKLIST:
			if (HasAnyFilter())
			{
				RebuildList();
			}
			else if (!GetPreferencesKey().IsEmpty()) // first time
			{
				SetViewNeedsTaskUpdate(nView);
			}
			break;

		case FTCV_UIEXTENSION1:
		case FTCV_UIEXTENSION2:
		case FTCV_UIEXTENSION3:
		case FTCV_UIEXTENSION4:
		case FTCV_UIEXTENSION5:
		case FTCV_UIEXTENSION6:
		case FTCV_UIEXTENSION7:
		case FTCV_UIEXTENSION8:
		case FTCV_UIEXTENSION9:
		case FTCV_UIEXTENSION10:
		case FTCV_UIEXTENSION11:
		case FTCV_UIEXTENSION12:
		case FTCV_UIEXTENSION13:
		case FTCV_UIEXTENSION14:
		case FTCV_UIEXTENSION15:
		case FTCV_UIEXTENSION16:
			// Note: By way of virtual functions CTabbedToDoCtrl::LoadTasks
			// will already have initialized the active view if it is an
			// extension so we only need to update if the tree actually
			// has a filter
			if (HasAnyFilter())
				RefreshExtensionFilter(nView);
			break;
		}
	}
	else if (HasAnyFilter())
	{
		RefreshFilter();
	}

	// restore previously visibility
	if (bHidden)
		ShowWindow(SW_HIDE);

	return TRUE;
}

BOOL CFilteredToDoCtrl::DelayLoad(const CString& sFilePath, COleDateTime& dtEarliestDue)
{
	if (CTabbedToDoCtrl::DelayLoad(sFilePath, dtEarliestDue))
	{
		LoadSettings();
		return TRUE;
	}
	
	// else
	return FALSE;
}

void CFilteredToDoCtrl::SaveSettings() const
{
	CPreferences prefs;
	m_filter.SaveFilter(prefs, GetPreferencesKey(_T("Filter")));
}

void CFilteredToDoCtrl::LoadSettings()
{
	if (HasStyle(TDCS_RESTOREFILTERS))
	{
		CPreferences prefs;
		m_filter.LoadFilter(prefs, GetPreferencesKey(_T("Filter")), m_aCustomAttribDefs);
	}
}

void CFilteredToDoCtrl::OnDestroy() 
{
	SaveSettings();

	CTabbedToDoCtrl::OnDestroy();
}

void CFilteredToDoCtrl::OnEditChangeDueTime()
{
	// need some special hackery to prevent a re-filter in the middle
	// of the user manually typing into the time field
	CDWordArray aSelTaskIDs;
	GetSelectedTaskIDs(aSelTaskIDs, FALSE);

	BOOL bNeedFullTaskUpdate = ModNeedsRefilter(TDCA_DUEDATE/*, FTCV_TASKTREE*/, aSelTaskIDs);
	
	if (bNeedFullTaskUpdate)
		m_styles[TDCS_REFILTERONMODIFY] = FALSE;
	
	CTabbedToDoCtrl::OnSelChangeDueTime();
	
	if (bNeedFullTaskUpdate)
		m_styles[TDCS_REFILTERONMODIFY] = TRUE;
}

BOOL CFilteredToDoCtrl::CopySelectedTasks() const
{
	// NOTE: we are overriding this function else
	// filtered out subtasks will not get copied
	if (!HasAnyFilter())
		return CTabbedToDoCtrl::CopySelectedTasks();

	// NOTE: We DON'T override GetSelectedTasks because
	// most often that only wants visible tasks
	if (!GetSelectedCount())
		return FALSE;
	
	ClearCopiedItem();
	
	CTaskFile tasks;
	PrepareTaskfileForTasks(tasks, TDCGETTASKS());
	
	// get selected tasks ordered, removing duplicate subtasks
	CHTIList selection;
	TSH().CopySelection(selection, TRUE, TRUE);
	
	// copy items
	POSITION pos = selection.GetHeadPosition();
	
	while (pos)
	{
		HTREEITEM hti = selection.GetNext(pos);
		DWORD dwTaskID = GetTaskID(hti);

		const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);
		const TODOITEM* pTDI = m_data.GetTask(dwTaskID);

		if (!pTDS || !pTDI)
			return FALSE;

		// add task
		HTASKITEM hTask = tasks.NewTask(pTDI->sTitle, NULL, dwTaskID, 0);
		ASSERT(hTask);
		
		if (!hTask)
			return FALSE;

		m_exporter.ExportTaskAttributes(pTDI, pTDS, tasks, hTask, TDCGT_ALL);

		// and subtasks
		m_exporter.ExportSubTasks(pTDS, tasks, hTask, TRUE);
	}
	
	// extra processing to identify the originally selected tasks
	// in case the user wants to paste as references
	// Note: references can always be pasted 'as references'
	CDWordArray aSelTasks;
	TSH().GetItemData(aSelTasks);

	tasks.SetSelectedTaskIDs(aSelTasks);
	
	// and their titles (including child dupes)
	CStringArray aTitles;
	
	VERIFY(TSH().CopySelection(selection, FALSE, TRUE));
	VERIFY(TSH().GetItemTitles(selection, aTitles));
	
	return CTaskClipboard::SetTasks(tasks, GetClipboardID(), Misc::FormatArray(aTitles, '\n'));
}

BOOL CFilteredToDoCtrl::ArchiveDoneTasks(TDC_ARCHIVE nFlags, BOOL bRemoveFlagged)
{
	if (CTabbedToDoCtrl::ArchiveDoneTasks(nFlags, bRemoveFlagged))
	{
		if (HasAnyFilter())
			RefreshFilter();

		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CFilteredToDoCtrl::ArchiveSelectedTasks(BOOL bRemove)
{
	if (CTabbedToDoCtrl::ArchiveSelectedTasks(bRemove))
	{
		if (HasAnyFilter())
			RefreshFilter();

		return TRUE;
	}

	// else
	return FALSE;
}

int CFilteredToDoCtrl::GetArchivableTasks(CTaskFile& tasks, BOOL bSelectedOnly) const
{
	if (bSelectedOnly || !HasAnyFilter())
		return CTabbedToDoCtrl::GetArchivableTasks(tasks, bSelectedOnly);

	// else process the entire data hierarchy
	return m_exporter.ExportCompletedTasks(tasks);
}

BOOL CFilteredToDoCtrl::RemoveArchivedTask(DWORD dwTaskID)
{
	ASSERT(m_data.HasTask(dwTaskID));
	
	// note: if the tasks does not exist in the tree then this is not a bug
	// if a filter is set
	HTREEITEM hti = m_taskTree.GetItem(dwTaskID);
	
	if (!hti && !HasAnyFilter())
	{
		ASSERT(0);
		return FALSE;
	}
	
	if (hti)
		m_taskTree.DeleteItem(hti);

	return m_data.DeleteTask(dwTaskID, TRUE); // TRUE == with undo

}

int CFilteredToDoCtrl::GetFilteredTasks(CTaskFile& tasks, const TDCGETTASKS& filter) const
{
	// synonym for GetTasks which always returns the filtered tasks
	return GetTasks(tasks, GetTaskView(), filter);
}

FILTER_SHOW CFilteredToDoCtrl::GetFilter(TDCFILTER& filter) const
{
	return m_filter.GetFilter(filter);
}

void CFilteredToDoCtrl::SetFilter(const TDCFILTER& filter)
{
	FTC_VIEW nView = GetTaskView();

	if (m_bDelayLoaded)
	{
		m_filter.SetFilter(filter);

		// mark everything needing refilter
		SetViewNeedsTaskUpdate(FTCV_TASKTREE);
		SetViewNeedsTaskUpdate(FTCV_TASKLIST);
		
		SetExtensionsNeedTaskUpdate();
	}
	else
	{
		BOOL bNeedFullTaskUpdate = !FilterMatches(filter);

		m_filter.SetFilter(filter);

		if (bNeedFullTaskUpdate)
			RefreshFilter();
	}

	ResetNowFilterTimer();
}
	
void CFilteredToDoCtrl::ClearFilter()
{
	if (m_filter.ClearFilter())
		RefreshFilter();

	ResetNowFilterTimer();
}

void CFilteredToDoCtrl::ToggleFilter()
{
	// PERMANENT LOGGING //////////////////////////////////////////////
	CScopedLogTimer log(_T("CFilteredToDoCtrl::ToggleFilter(%s)"), (m_filter.HasAnyFilter() ? _T("off") : _T("on")));
	///////////////////////////////////////////////////////////////////

	if (m_filter.ToggleFilter())
		RefreshFilter();

	ResetNowFilterTimer();
}

UINT CFilteredToDoCtrl::GetTaskCount(UINT* pVisible) const
{
	if (pVisible)
	{
		if (InListView())
			*pVisible = (m_taskList.GetItemCount() - m_taskList.GetGroupCount());
		else
			*pVisible = m_taskTree.GetItemCount();
	}

	return CTabbedToDoCtrl::GetTaskCount();
}

int CFilteredToDoCtrl::FindTasks(const SEARCHPARAMS& params, CResultArray& aResults) const
{
	if (params.bIgnoreFilteredOut)
		return CTabbedToDoCtrl::FindTasks(params, aResults);
	
	// else all tasks
	return m_matcher.FindTasks(params, aResults, HasDueTodayColor());
}

BOOL CFilteredToDoCtrl::HasAdvancedFilter() const 
{ 
	return m_filter.HasAdvancedFilter(); 
}

CString CFilteredToDoCtrl::GetAdvancedFilterName() const 
{ 
	return m_filter.GetAdvancedFilterName();
}

DWORD CFilteredToDoCtrl::GetAdvancedFilterFlags() const 
{ 
	if (HasAdvancedFilter())
		return m_filter.GetFilterFlags();

	// else
	return 0L;
}

BOOL CFilteredToDoCtrl::SetAdvancedFilter(const TDCADVANCEDFILTER& filter)
{
	if (m_filter.SetAdvancedFilter(filter))
	{
		if (m_bDelayLoaded)
		{
			// mark everything needing refilter
			SetViewNeedsTaskUpdate(FTCV_TASKTREE);
			SetViewNeedsTaskUpdate(FTCV_TASKLIST);

			SetExtensionsNeedTaskUpdate();
		}
		else
		{
			RefreshFilter();
		}

		return TRUE;
	}

	ASSERT(0);
	return FALSE;
}

BOOL CFilteredToDoCtrl::FilterMatches(const TDCFILTER& filter, LPCTSTR szCustom, DWORD dwCustomFlags) const
{
	return m_filter.FilterMatches(filter, szCustom, dwCustomFlags);
}

void CFilteredToDoCtrl::RefreshFilter() 
{
	CSaveFocus sf;

	RefreshTreeFilter(); // always

	FTC_VIEW nView = GetTaskView();

	switch (nView)
	{
	case FTCV_TASKTREE:
	case FTCV_UNSET:
		SetViewNeedsTaskUpdate(FTCV_TASKLIST);
		SetExtensionsNeedTaskUpdate();
		break;

	case FTCV_TASKLIST:
		RebuildList();
		SetExtensionsNeedTaskUpdate();
		break;

	case FTCV_UIEXTENSION1:
	case FTCV_UIEXTENSION2:
	case FTCV_UIEXTENSION3:
	case FTCV_UIEXTENSION4:
	case FTCV_UIEXTENSION5:
	case FTCV_UIEXTENSION6:
	case FTCV_UIEXTENSION7:
	case FTCV_UIEXTENSION8:
	case FTCV_UIEXTENSION9:
	case FTCV_UIEXTENSION10:
	case FTCV_UIEXTENSION11:
	case FTCV_UIEXTENSION12:
	case FTCV_UIEXTENSION13:
	case FTCV_UIEXTENSION14:
	case FTCV_UIEXTENSION15:
	case FTCV_UIEXTENSION16:
		SetViewNeedsTaskUpdate(FTCV_TASKLIST);
		SetExtensionsNeedTaskUpdate(TRUE);
		RefreshExtensionFilter(nView, TRUE);
		SyncExtensionSelectionToTree(nView);
		break;
	}
}

void CFilteredToDoCtrl::RefreshTreeFilter() 
{
	if (m_data.GetTaskCount())
	{
		// grab the selected items for the filtering
		m_taskTree.GetSelectedTaskIDs(m_aSelectedTaskIDsForFiltering, FALSE);
		
		// rebuild the tree
		RebuildTree();
		
		// redo last sort
		if (InTreeView() && IsSorting())
		{
			Resort();
			m_bTreeNeedResort = FALSE;
		}
		else
		{
			m_bTreeNeedResort = TRUE;
		}
	}
	
	// modify the tree prompt depending on whether there is a filter set
	if (HasAnyFilter())
		m_taskTree.SetWindowPrompt(CEnString(IDS_TDC_FILTEREDTASKLISTPROMPT));
	else
		m_taskTree.SetWindowPrompt(CEnString(IDS_TDC_TASKLISTPROMPT));
}

void CFilteredToDoCtrl::RebuildList(BOOL bChangeGroup, TDC_COLUMN nNewGroupBy)
{
	if (m_bIgnoreListRebuild)
		return;

	// else
	CTabbedToDoCtrl::RebuildList(bChangeGroup, nNewGroupBy);
}

HTREEITEM CFilteredToDoCtrl::RebuildTree(const void* pContext)
{
	ASSERT(pContext == NULL);

	// build a find query that matches the filter
	if (HasAnyFilter())
	{
		SEARCHPARAMS params;
		m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);

		return CTabbedToDoCtrl::RebuildTree(&params);
	}

	// else
	return CTabbedToDoCtrl::RebuildTree(pContext);
}

BOOL CFilteredToDoCtrl::WantAddTaskToTree(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, const void* pContext) const
{
	BOOL bWantTask = CTabbedToDoCtrl::WantAddTaskToTree(pTDI, pTDS, pContext);

#ifdef _DEBUG
	DWORD dwTaskID = pTDS->GetTaskID();
	DWORD dwParentID = pTDS->GetParentTaskID();
#endif
	
	if (bWantTask && (pContext != NULL)) // it's a filter
	{
		const SEARCHPARAMS* pFilter = static_cast<const SEARCHPARAMS*>(pContext);
		SEARCHRESULT result;
		
		// special case: selected item
		if (pFilter->HasAttribute(TDCA_SELECTION))
		{
			// check completion
			if (pFilter->bIgnoreDone && m_calculator.IsTaskDone(pTDI, pTDS))
			{
				bWantTask = FALSE;
			}
			else
			{
				bWantTask = Misc::HasT(pTDS->GetTaskID(), m_aSelectedTaskIDsForFiltering);

				// check parents
				if (!bWantTask && pFilter->bWantAllSubtasks)
				{
					TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();

					while (pTDSParent && !pTDSParent->IsRoot() && !bWantTask)
					{
						bWantTask = Misc::HasT(pTDSParent->GetTaskID(), m_aSelectedTaskIDsForFiltering);
						pTDSParent = pTDSParent->GetParentTask();
					}
				}
			}
		}
		else // rest of attributes
		{
			bWantTask = m_matcher.TaskMatches(pTDI, pTDS, *pFilter, result, HasDueTodayColor());
		}

		if (bWantTask && pTDS->HasSubTasks())
		{
			// NOTE: the only condition under which this method is called for
			// a parent is if none of its subtasks matched the filter.
			//
			// So if we're a parent and match the filter we need to do an extra check
			// to see if what actually matched was the absence of attributes
			//
			// eg. if the parent category is "" and the filter rule is 
			// TDCA_CATEGORY is (FOP_NOT_SET or FOP_NOT_INCLUDES or FOP_NOT_EQUAL) 
			// then we don't treat this as a match.
			//
			// The attributes to check are:
			//  Category
			//  Status
			//  Alloc To
			//  Alloc By
			//  Version
			//  Priority
			//  Risk
			//  Tags
			
			int nNumRules = pFilter->aRules.GetSize();
			
			for (int nRule = 0; nRule < nNumRules && bWantTask; nRule++)
			{
				const SEARCHPARAM& sp = pFilter->aRules[nRule];

				if (!sp.OperatorIs(FOP_NOT_EQUALS) && 
					!sp.OperatorIs(FOP_NOT_INCLUDES) && 
					!sp.OperatorIs(FOP_NOT_SET))
				{
					continue;
				}
				
				// else check for empty parent attributes
				switch (sp.GetAttribute())
				{
				case TDCA_ALLOCTO:
					bWantTask = (pTDI->aAllocTo.GetSize() > 0);
					break;
					
				case TDCA_ALLOCBY:
					bWantTask = !pTDI->sAllocBy.IsEmpty();
					break;
					
				case TDCA_VERSION:
					bWantTask = !pTDI->sVersion.IsEmpty();
					break;
					
				case TDCA_STATUS:
					bWantTask = !pTDI->sStatus.IsEmpty();
					break;
					
				case TDCA_CATEGORY:
					bWantTask = (pTDI->aCategories.GetSize() > 0);
					break;
					
				case TDCA_TAGS:
					bWantTask = (pTDI->aTags.GetSize() > 0);
					break;
					
				case TDCA_PRIORITY:
					bWantTask = (pTDI->nPriority != FM_NOPRIORITY);
					break;
					
				case TDCA_RISK:
					bWantTask = (pTDI->nRisk != FM_NORISK);
					break;
				}
			}
		}
	}
	
	return bWantTask; 
}

void CFilteredToDoCtrl::RefreshExtensionFilter(FTC_VIEW nView, BOOL bShowProgress)
{
	CWaitCursor cursor;

	IUIExtensionWindow* pExtWnd = GetCreateExtensionWnd(nView);
	ASSERT(pExtWnd);

	if (pExtWnd)
	{
		VIEWDATA* pData = GetViewData(nView);
		ASSERT(pData);

		if (bShowProgress)
			BeginExtensionProgress(pData, IDS_UPDATINGTABBEDVIEW);
		
		// clear all update flag
		pData->bNeedFullTaskUpdate = FALSE;

		// update view with filtered tasks
		CTaskFile tasks;
		GetAllTasksForExtensionViewUpdate(pData->mapWantedAttrib, tasks);

		UpdateExtensionView(pExtWnd, tasks, IUI_ALL); 
		
		if (bShowProgress)
			EndExtensionProgress();
	}
}

void CFilteredToDoCtrl::OnStylesUpdated(const CTDCStyleMap& styles)
{
	// If we're going to refilter anyway we ignore our
	// base class's possible update of the listview in
	// response to TDCS_ALWAYSHIDELISTPARENTS changing
	BOOL bNeedRefilter = StyleChangesNeedRefilter(styles);

	{
		ASSERT(!m_bIgnoreListRebuild);
		CAutoFlag af(m_bIgnoreListRebuild, bNeedRefilter);

		CTabbedToDoCtrl::OnStylesUpdated(styles);
	}
	
	if (bNeedRefilter)
		RefreshFilter();
}

void CFilteredToDoCtrl::SetDueTaskColors(COLORREF crDue, COLORREF crDueToday)
{
	// See if we need to refilter
	BOOL bHadDueToday = m_taskTree.HasDueTodayColor();

	CTabbedToDoCtrl::SetDueTaskColors(crDue, crDueToday);

	if (bHadDueToday != m_taskTree.HasDueTodayColor())
	{
		// Because the 'Due Today' colour effectively alters 
		// a task's priority we can treat it as a priority edit
		if (m_filter.ModNeedsRefilter(TDCA_PRIORITY, m_aCustomAttribDefs))
			RefreshFilter();
	}
}

BOOL CFilteredToDoCtrl::CreateNewTask(LPCTSTR szText, TDC_INSERTWHERE nWhere, BOOL bEditText, DWORD dwDependency)
{
	if (CTabbedToDoCtrl::CreateNewTask(szText, nWhere, bEditText, dwDependency))
	{
		SetViewNeedsTaskUpdate(FTCV_TASKLIST, !InListView());
		SetExtensionsNeedTaskUpdate(TRUE, GetTaskView());

		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CFilteredToDoCtrl::CanCreateNewTask(TDC_INSERTWHERE nInsertWhere) const
{
	return CTabbedToDoCtrl::CanCreateNewTask(nInsertWhere);
}

void CFilteredToDoCtrl::SetModified(const CTDCAttributeMap& mapAttribIDs, const CDWordArray& aModTaskIDs, BOOL bAllowResort)
{
	BOOL bTreeRefiltered = FALSE, bListRefiltered = FALSE;

	if (ModsNeedRefilter(mapAttribIDs, aModTaskIDs))
	{
		// This will also refresh the list view if it is active
		RefreshFilter();

		// Note: This will also have refreshed the active view
		bTreeRefiltered = TRUE;
		bListRefiltered = InListView();
	}

	// This may cause either the list or one of the extensions to be rebuilt
	// we set flags and ignore it
	CAutoFlag af(m_bIgnoreListRebuild, bListRefiltered);
	CAutoFlag af2(m_bIgnoreExtensionUpdate, bTreeRefiltered);

	CTabbedToDoCtrl::SetModified(mapAttribIDs, aModTaskIDs, bAllowResort);

	if ((bListRefiltered || bTreeRefiltered) && mapAttribIDs.Has(TDCA_UNDO) && aModTaskIDs.GetSize())
	{
		// Restore the selection at the time of the undo if possible
		TDCSELECTIONCACHE cache;
		CacheTreeSelection(cache);
		
		if (!SelectTasks(aModTaskIDs, FALSE))
			RestoreTreeSelection(cache);

		SyncActiveViewSelectionToTree();
	}
}

void CFilteredToDoCtrl::EndTimeTracking(BOOL bAllowConfirm, BOOL bNotify)
{
	BOOL bWasTimeTracking = IsActivelyTimeTracking();
	
	CTabbedToDoCtrl::EndTimeTracking(bAllowConfirm, bNotify);
	
	// do we need to refilter?
	if (bWasTimeTracking && m_filter.HasAdvancedFilter() && m_filter.HasAdvancedFilterAttribute(TDCA_TIMESPENT))
	{
		RefreshFilter();
	}
}

BOOL CFilteredToDoCtrl::ModsNeedRefilter(const CTDCAttributeMap& mapAttribIDs, const CDWordArray& aModTaskIDs) const
{
	if (!m_filter.HasAnyFilter())
		return FALSE;

	POSITION pos = mapAttribIDs.GetStartPosition();

	while (pos)
	{
		if (ModNeedsRefilter(mapAttribIDs.GetNext(pos), aModTaskIDs))
			return TRUE;
	}

	return FALSE;
}

BOOL CFilteredToDoCtrl::ModNeedsRefilter(TDC_ATTRIBUTE nAttrib, const CDWordArray& aModTaskIDs) const
{
	// sanity checks
	if ((nAttrib == TDCA_NONE) || !HasStyle(TDCS_REFILTERONMODIFY))
		return FALSE;

	if (!m_filter.HasAnyFilter())
		return FALSE;

	// we only need to refilter if the modified attribute
	// actually affects the filter
	BOOL bNeedRefilter = m_filter.ModNeedsRefilter(nAttrib, m_aCustomAttribDefs);

	if (!bNeedRefilter)
	{
		// 'Other' attributes
		switch (nAttrib)
		{
		case TDCA_NEWTASK: // handled in CreateNewTask
		case TDCA_DELETE:
		case TDCA_POSITION:
		case TDCA_POSITION_SAMEPARENT:
		case TDCA_POSITION_DIFFERENTPARENT:
		case TDCA_SELECTION: 
			return FALSE;
			
		case TDCA_UNDO:
		case TDCA_PASTE:
		case TDCA_MERGE:
			return TRUE;
		}
	}
	else if (aModTaskIDs.GetSize() == 1)
	{
		DWORD dwModTaskID = aModTaskIDs[0];

		// VERY SPECIAL CASE
		// The task being time tracked has been filtered out
		// in which case we don't need to check if it matches
		if (m_timeTracking.IsTrackingTask(dwModTaskID))
		{
			if (m_taskTree.GetItem(dwModTaskID) == NULL)
			{
				ASSERT(HasTask(dwModTaskID));
				ASSERT(nAttrib == TDCA_TIMESPENT);

				return FALSE;
			}
			// else fall thru
		}

		// Finally, if this was a simple task edit we can just test to 
		// see if the modified task still matches the filter.
		SEARCHPARAMS params;
		SEARCHRESULT result;

		m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);

		BOOL bMatchesFilter = m_matcher.TaskMatches(dwModTaskID, params, result, FALSE);
		BOOL bTreeHasItem = (m_taskTree.GetItem(dwModTaskID) != NULL);

		bNeedRefilter = ((bMatchesFilter && !bTreeHasItem) || (!bMatchesFilter && bTreeHasItem));
		
		// extra handling for 'Find Tasks' filters 
		if (bNeedRefilter && HasAdvancedFilter())
		{
			// don't refilter on Time Spent if time tracking
			bNeedRefilter = !(nAttrib == TDCA_TIMESPENT && IsActivelyTimeTracking());
		}
	}

	return bNeedRefilter;
}

BOOL CFilteredToDoCtrl::StyleChangesNeedRefilter(const CTDCStyleMap& styles) const
{
	// sanity check
	if (!HasAnyFilter())
		return FALSE;

	CTDCAttributeMap mapAttribAffected;
	POSITION pos = styles.GetStartPosition();

	while (pos)
	{
		switch (styles.GetNext(pos))
		{
		case TDCS_NODUEDATEISDUETODAYORSTART:
		case TDCS_USEEARLIESTDUEDATE:
		case TDCS_USELATESTDUEDATE:
			mapAttribAffected.Add(TDCA_DUEDATE);
			break;

		case TDCS_USEEARLIESTSTARTDATE:
		case TDCS_USELATESTSTARTDATE:
			mapAttribAffected.Add(TDCA_STARTDATE);
			break;

		case TDCS_CALCREMAININGTIMEBYDUEDATE:
		case TDCS_CALCREMAININGTIMEBYSPENT:
		case TDCS_CALCREMAININGTIMEBYPERCENT:
			// Not supported
			break;

		case TDCS_DUEHAVEHIGHESTPRIORITY:
		case TDCS_DONEHAVELOWESTPRIORITY:
		case TDCS_USEHIGHESTPRIORITY:
		case TDCS_INCLUDEDONEINPRIORITYCALC:
			mapAttribAffected.Add(TDCA_PRIORITY);
			break;

		case TDCS_DONEHAVELOWESTRISK:
		case TDCS_USEHIGHESTRISK:
		case TDCS_INCLUDEDONEINRISKCALC:
			mapAttribAffected.Add(TDCA_RISK);
			break;

		case TDCS_TREATSUBCOMPLETEDASDONE:
			{
				mapAttribAffected.Add(TDCA_DONEDATE);

				if (styles.HasStyle(TDCS_DONEHAVELOWESTPRIORITY) || 
					styles.HasStyle(TDCS_INCLUDEDONEINPRIORITYCALC))
				{
					mapAttribAffected.Add(TDCA_PRIORITY);
				}

				if (styles.HasStyle(TDCS_DONEHAVELOWESTRISK) ||
					styles.HasStyle(TDCS_INCLUDEDONEINRISKCALC))
				{
					mapAttribAffected.Add(TDCA_PRIORITY);
				}

				if (styles.HasStyle(TDCS_INCLUDEDONEINAVERAGECALC))
				{
					mapAttribAffected.Add(TDCA_PERCENT);
				}
			}
			break;

		case TDCS_USEPERCENTDONEINTIMEEST:
			mapAttribAffected.Add(TDCA_TIMEEST);
			break;

		case TDCS_INCLUDEDONEINAVERAGECALC:
		case TDCS_WEIGHTPERCENTCALCBYNUMSUB:
		case TDCS_AVERAGEPERCENTSUBCOMPLETION:
		case TDCS_AUTOCALCPERCENTDONE:
			mapAttribAffected.Add(TDCA_PERCENT);
			break;

		case TDCS_HIDESTARTDUEFORDONETASKS:
			//mapAttribAffected.Add(TDCA_);
			break;
		}
	}
	
	return ModsNeedRefilter(mapAttribAffected, CDWordArray());
}

void CFilteredToDoCtrl::Sort(TDC_COLUMN nBy, BOOL bAllowToggle)
{
	CTabbedToDoCtrl::Sort(nBy, bAllowToggle);
}

void CFilteredToDoCtrl::OnTimerMidnight()
{
	CTabbedToDoCtrl::OnTimerMidnight();

	// don't re-filter delay-loaded tasklists
	if (IsDelayLoaded())
		return;

	BOOL bRefilter = FALSE;
	TDCFILTER filter;
	
	if (m_filter.GetFilter(filter) == FS_ADVANCED)
	{
		bRefilter = (m_filter.HasAdvancedFilterAttribute(TDCA_STARTDATE) || 
						m_filter.HasAdvancedFilterAttribute(TDCA_DUEDATE));
	}
	else
	{
		bRefilter = (((filter.nStartBy != FD_NONE) && (filter.nStartBy != FD_ANY)) ||
					((filter.nDueBy != FD_NONE) && (filter.nDueBy != FD_ANY)));
	}
	
	if (bRefilter)
		RefreshFilter();
}

void CFilteredToDoCtrl::ResetNowFilterTimer()
{
	if (m_filter.HasNowFilter())
	{
		SetTimer(TIMER_NOWFILTER, ONE_MINUTE, NULL);
		return;
	}

	// all else
	KillTimer(TIMER_NOWFILTER);
}

void CFilteredToDoCtrl::OnTimer(UINT nIDEvent) 
{
	AF_NOREENTRANT;
	
	switch (nIDEvent)
	{
	case TIMER_NOWFILTER:
		OnTimerNow();
		return;
	}

	CTabbedToDoCtrl::OnTimer(nIDEvent);
}

void CFilteredToDoCtrl::OnTimerNow()
{
	// Since this timer gets called every minute we have to
	// find an efficient way of detecting tasks that are
	// currently hidden but need to be shown
	
	// So first thing we do is find reasons not to:
	
	// We are hidden
	if (!IsWindowVisible())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (Window not visible)\n"));
		return;
	}
	
	// We're already displaying all tasks
	if (m_taskTree.GetItemCount() == m_data.GetTaskCount())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (All tasks showing)\n"));
		return;
	}
	
	// App is minimized or hidden
	if (AfxGetMainWnd()->IsIconic() || !AfxGetMainWnd()->IsWindowVisible())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (App not visible)\n"));
		return;
	}
	
	// App is not the foreground window
	if (GetForegroundWindow() != AfxGetMainWnd())
	{
		TRACE(_T("CFilteredToDoCtrl::OnTimerNow eaten (App not active)\n"));
		return;
	}
	
	// iterate the full data looking for items not in the
	// tree and test them for inclusion in the filter
	ASSERT(m_taskTree.TreeItemMap().GetCount() < m_data.GetTaskCount());
	
	SEARCHPARAMS params;
	m_filter.BuildFilterQuery(params, m_aCustomAttribDefs);
	
	const TODOSTRUCTURE* pTDS = m_data.GetStructure();
	ASSERT(pTDS);
	
	if (FindNewNowFilterTasks(pTDS, params, m_taskTree.TreeItemMap()))
	{
		TDC_ATTRIBUTE nNowAttrib;

		if (m_filter.HasNowFilter(nNowAttrib))
		{
			BOOL bRefilter = FALSE;
		
			switch (nNowAttrib)
			{
			case TDCA_DUEDATE:
				bRefilter = (AfxMessageBox(CEnString(IDS_DUEBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				break;

			case TDCA_STARTDATE:
				bRefilter = (AfxMessageBox(CEnString(IDS_STARTBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				break;

			default:
				if (TDCCUSTOMATTRIBUTEDEFINITION::IsCustomAttribute(nNowAttrib))
				{
					// TODO
					//bRefilter = (AfxMessageBox(CEnString(IDS_CUSTOMBYNOW_CONFIRMREFILTER), MB_YESNO | MB_ICONQUESTION) == IDYES);
				}
				else
				{
					ASSERT(0);
				}
			}
		
			if (bRefilter)
			{
				RefreshFilter();
			}
			else // make the timer 10 minutes so we don't re-prompt them too soon
			{
				SetTimer(TIMER_NOWFILTER, TEN_MINUTES, NULL);
			}
		}
	}
}

BOOL CFilteredToDoCtrl::FindNewNowFilterTasks(const TODOSTRUCTURE* pTDS, const SEARCHPARAMS& params, const CHTIMap& htiMap) const
{
	ASSERT(pTDS);

	// process task
	if (!pTDS->IsRoot())
	{
		// is the task invisible?
		HTREEITEM htiDummy;
		DWORD dwTaskID = pTDS->GetTaskID();

		if (!htiMap.Lookup(dwTaskID, htiDummy))
		{
			// does the task match the current filter
			SEARCHRESULT result;

			// This will handle custom and 'normal' filters
			if (m_matcher.TaskMatches(dwTaskID, params, result, FALSE))
				return TRUE;
		}
	}

	// then children
	for (int nTask = 0; nTask < pTDS->GetSubTaskCount(); nTask++)
	{
		if (FindNewNowFilterTasks(pTDS->GetSubTask(nTask), params, htiMap))
			return TRUE;
	}

	// no new tasks
	return FALSE;
}

BOOL CFilteredToDoCtrl::GetAllTasksForExtensionViewUpdate(const CTDCAttributeMap& mapAttrib, CTaskFile& tasks) const
{
	if (m_bIgnoreExtensionUpdate)
	{
		return FALSE;
	}

	// Special case: No filter is set -> All tasks (v much faster)
	if (!HasAnyFilter())
	{
		PrepareTaskfileForTasks(tasks, TDCGT_ALL);

		if (CTabbedToDoCtrl::GetAllTasks(tasks))
		{
			AddGlobalsToTaskFile(tasks, mapAttrib);
			tasks.SetAvailableAttributes(mapAttrib);
			
			return TRUE;
		}

		// else
		return FALSE;
	}

	// else
	return CTabbedToDoCtrl::GetAllTasksForExtensionViewUpdate(mapAttrib, tasks);
}

void CFilteredToDoCtrl::SetColumnFieldVisibility(const TDCCOLEDITFILTERVISIBILITY& vis)
{
	CTabbedToDoCtrl::SetColumnFieldVisibility(vis);

	m_visColEditFilter = vis;
}

void CFilteredToDoCtrl::GetColumnFieldVisibility(TDCCOLEDITFILTERVISIBILITY& vis) const
{
	vis = m_visColEditFilter;
}

const CTDCColumnIDMap& CFilteredToDoCtrl::GetVisibleColumns() const
{
	ASSERT(m_visColEditFilter.GetVisibleColumns().MatchAll(m_visColEdit.GetVisibleColumns()));

	return m_visColEditFilter.GetVisibleColumns();
}

const CTDCAttributeMap& CFilteredToDoCtrl::GetVisibleEditFields() const
{
	ASSERT(m_visColEditFilter.GetVisibleEditFields().MatchAll(m_visColEdit.GetVisibleEditFields()));

	return m_visColEditFilter.GetVisibleEditFields();
}

const CTDCAttributeMap& CFilteredToDoCtrl::GetVisibleFilterFields() const
{
	return m_visColEditFilter.GetVisibleFilterFields();
}

void CFilteredToDoCtrl::SaveAttributeVisibility(CTaskFile& tasks) const
{
	if (HasStyle(TDCS_SAVEUIVISINTASKLIST))
		tasks.SetAttributeVisibility(m_visColEditFilter);
}

void CFilteredToDoCtrl::SaveAttributeVisibility(CPreferences& prefs) const
{
	m_visColEditFilter.Save(prefs, GetPreferencesKey());
}

void CFilteredToDoCtrl::LoadAttributeVisibility(const CTaskFile& tasks, const CPreferences& prefs)
{
	// attrib visibility can be stored inside the file or the preferences
	TDCCOLEDITFILTERVISIBILITY vis;

	if (tasks.GetAttributeVisibility(vis))
	{
		// update style to match
		m_styles[TDCS_SAVEUIVISINTASKLIST] = TRUE;
	}
	else if (!vis.Load(prefs, GetPreferencesKey()))
	{
		vis = m_visColEditFilter;
	}

	SetColumnFieldVisibility(vis);
}

DWORD CFilteredToDoCtrl::MergeNewTaskIntoTree(const CTaskFile& tasks, HTASKITEM hTask, DWORD dwParentTaskID, BOOL bAndSubtasks)
{
	// If the parent has been filtered out we just add 
	// directly to the data model
	if (dwParentTaskID && !m_taskTree.GetItem(dwParentTaskID))
		return MergeNewTaskIntoTree(tasks, hTask, dwParentTaskID, 0, bAndSubtasks);

	// else
	return CTabbedToDoCtrl::MergeNewTaskIntoTree(tasks, hTask, dwParentTaskID, bAndSubtasks);
}

DWORD CFilteredToDoCtrl::MergeNewTaskIntoTree(const CTaskFile& tasks, HTASKITEM hTask, DWORD dwParentTaskID, DWORD dwPrevSiblingID, BOOL bAndSubtasks)
{
	TODOITEM* pTDI = m_data.NewTask(tasks, hTask);

	DWORD dwTaskID = m_dwNextUniqueID++;
	m_data.AddTask(dwTaskID, pTDI, dwParentTaskID, dwPrevSiblingID);

	if (bAndSubtasks)
	{
		HTASKITEM hSubtask = tasks.GetFirstTask(hTask);
		DWORD dwSubtaskID = 0;

		while (hSubtask)
		{
			dwSubtaskID = MergeNewTaskIntoTree(tasks, hSubtask, dwTaskID, dwSubtaskID, TRUE);
			hSubtask = tasks.GetNextTask(hSubtask);
		}
	}

	return dwTaskID;
}

DWORD CFilteredToDoCtrl::RecreateRecurringTaskInTree(const CTaskFile& task, const COleDateTime& dtNext, BOOL bDueDate)
{
	// We only need handle this if the existing task has been filtered out
	DWORD dwTaskID = task.GetTaskID(task.GetFirstTask());

	if (HasAnyFilter() && (m_taskTree.GetItem(dwTaskID) == NULL))
	{
		// Merge task into data structure after the existing task
		DWORD dwParentID = m_data.GetTaskParentID(dwTaskID);
		DWORD dwNewTaskID = MergeNewTaskIntoTree(task, task.GetFirstTask(), dwParentID, dwTaskID, TRUE);

		InitialiseNewRecurringTask(dwTaskID, dwNewTaskID, dtNext, bDueDate);
		RefreshFilter();
		
		ASSERT(m_taskTree.GetItem(dwNewTaskID) != NULL);
		return dwNewTaskID;
	}

	// all else
	return CTabbedToDoCtrl::RecreateRecurringTaskInTree(task, dtNext, bDueDate);
}

