// GanttTreeList.h: interface for the CGanttTreeList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GANTTTREELISTCTRL_H__016B94F3_1D28_4532_97EF_95F1D9D5CE55__INCLUDED_)
#define AFX_GANTTTREELISTCTRL_H__016B94F3_1D28_4532_97EF_95F1D9D5CE55__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Ganttstruct.h"

#include "..\shared\TreeListCtrl.h"
#include "..\shared\mapex.h"

#include "..\Interfaces\itasklist.h"
#include "..\Interfaces\iuiextension.h"

/////////////////////////////////////////////////////////////////////////////

#define TVN_KEYUP (TVN_FIRST-16)

/////////////////////////////////////////////////////////////////////////////

class CThemed;

/////////////////////////////////////////////////////////////////////////////

class CGanttCtrl : public CTreeListCtrl  
{
public:
	CGanttCtrl();
	virtual ~CGanttCtrl();

	BOOL SaveToImage(CBitmap& bmImage);
	BOOL SetFont(HFONT hFont, BOOL bRedraw = TRUE);

	void UpdateTasks(const ITaskList* pTasks, IUI_UPDATETYPE nUpdate);
	bool PrepareNewTask(ITaskList* pTask) const;
	BOOL CancelOperation();

	DWORD GetSelectedTaskID() const;
	BOOL SelectTask(DWORD dwTaskID);
	BOOL SelectTask(IUI_APPCOMMAND nCmd, const IUISELECTTASK& select);
	BOOL GetSelectedTaskDates(COleDateTime& dtStart, COleDateTime& dtDue) const;
	DWORD GetNextTask(DWORD dwTaskID, IUI_APPCOMMAND nCmd) const;
	int GetTaskCount() const { return (int)m_tree.GetCount(); }

	BOOL CanMoveSelectedTask(const IUITASKMOVE& move) const;
	BOOL MoveSelectedTask(const IUITASKMOVE& move);
	BOOL IsMovingTask() const { return m_bMovingTask; }

	BOOL GetSelectedTaskDependencies(CDWordArray& aDepends) const;
	BOOL SetSelectedTaskDependencies(const CDWordArray& aDepends);

	DWORD HitTestTask(const CPoint& ptScreen) const;
	void ExpandItem(HTREEITEM hti, BOOL bExpand = TRUE, BOOL bAndChildren = FALSE);

	void ResizeAttributeColumnsToFit(BOOL bForce = FALSE);
	void AdjustSplitterToFitAttributeColumns();

	BOOL ZoomIn(BOOL bIn = TRUE);
	BOOL ZoomBy(int nAmount);

	void Sort(GTLC_COLUMN nBy, BOOL bAscending);
	void Sort(const GANTTSORTCOLUMNS multi);
	GTLC_COLUMN GetSortColumn() const { return m_sort.single.nBy; }
	BOOL GetSortAscending() const { return m_sort.single.bAscending; }

	GTLC_MONTH_DISPLAY GetMonthDisplay() const { return m_nMonthDisplay; }
	BOOL SetMonthDisplay(GTLC_MONTH_DISPLAY nNewDisplay);
	BOOL CanSetMonthDisplay(GTLC_MONTH_DISPLAY nDisplay) const;
	BOOL ValidateMonthDisplay(GTLC_MONTH_DISPLAY& nDisplay) const;
	void ValidateMonthDisplay();

	void ScrollToToday();
	void ScrollToSelectedTask();
	void ScrollToTask(DWORD dwTaskID);

	void SetOption(DWORD dwOption, BOOL bSet = TRUE);
	BOOL HasOption(DWORD dwOption) const { return (m_dwOptions & dwOption); }

	void SetTodayColor(COLORREF crToday);
	void SetWeekendColor(COLORREF crWeekend);
	void SetNonWorkingHoursColor(COLORREF crNonWorkingHoursColor);
	void SetDefaultColor(COLORREF crDefault);
	void SetParentColoring(GTLC_PARENTCOLORING nOption, COLORREF color);
	void SetMilestoneTag(const CString& sTag);
	void SetReadOnly(bool bReadOnly);
	
	void SetSnapMode(GTLC_SNAPMODE nSnap) { m_nSnapMode = nSnap; }
	GTLC_SNAPMODE GetSnapMode() const;

	BOOL BeginDependencyEdit(IGanttDependencyEditor* pDependEdit);
	void OnEndDepedencyEdit();

	void GetColumnWidths(CIntArray& aTreeWidths, CIntArray& aListWidths) const;
	BOOL SetColumnWidths(const CIntArray& aTreeWidths, const CIntArray& aListWidths);
	BOOL SetTrackedColumns(const CIntArray& aTreeTracked, const CIntArray& aListTracked);
	void GetTrackedColumns(CIntArray& aTreeTracked, CIntArray& aListTracked) const;

	void ClearActiveDateRange();
	BOOL SetActiveDateRange(const GANTTDATERANGE& dtRange);
	BOOL GetActiveDateRange(GANTTDATERANGE& dtRange) const;
	BOOL GetDataDateRange(GANTTDATERANGE& dtRange) const;

	static BOOL WantEditUpdate(TDC_ATTRIBUTE nAttrib);
	static BOOL WantSortUpdate(TDC_ATTRIBUTE nAttrib);
	static TDC_ATTRIBUTE MapColumnToAttribute(GTLC_COLUMN nCol);
	static GTLC_COLUMN MapAttributeToColumn(TDC_ATTRIBUTE nAttrib);

protected:
	BOOL m_bMovingTask;
	BOOL m_bReadOnly;

	GANTTDATERANGE m_dtDataRange, m_dtActiveRange;
	GANTTITEM m_giPreDrag;
	GANTTSORT m_sort;

	IGanttDependencyEditor* m_pDependEdit;
	CMap<GTLC_MONTH_DISPLAY, GTLC_MONTH_DISPLAY, int, int> m_mapMinMonthWidths;

	COLORREF m_crParent, m_crDefault;
	COLORREF m_crToday, m_crWeekend, m_crNonWorkingHoursColor;
	COleDateTime m_dtDragMin;
	CPoint m_ptDragStart, m_ptLastDependPick;
	DWORD m_dwOptions;
	DWORD m_dwMaxTaskID;
	GTLC_MONTH_DISPLAY m_nMonthDisplay;
	GTLC_PARENTCOLORING m_nParentColoring;
	int m_nMonthWidth;
	CString m_sMilestoneTag;
	GTLC_DRAG m_nDragging;

	CGanttItemMap m_data;

private:
	mutable GTLC_SNAPMODE m_nSnapMode;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClickTreeHeader(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginEditTreeLabel(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

protected:
	// virtual overrides
	LRESULT ScWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

	LRESULT OnTreeCustomDraw(NMTVCUSTOMDRAW* pTVCD);
	LRESULT OnListCustomDraw(NMLVCUSTOMDRAW* pLVCD);
	LRESULT OnHeaderCustomDraw(NMCUSTOMDRAW* pNMCD);
			
	BOOL OnTreeSelectionChange(NMTREEVIEW* pNMTV);
	BOOL OnDragDropItem(const TLCITEMMOVE& move);
	UINT OnDragOverItem(UINT nCursor);
	BOOL OnItemCheckChange(HTREEITEM hti);

	BOOL OnTreeLButtonDown(UINT nFlags, CPoint point);
	BOOL OnTreeLButtonUp(UINT nFlags, CPoint point);
	BOOL OnTreeMouseMove(UINT nFlags, CPoint point);
	BOOL OnListLButtonDown(UINT nFlags, CPoint point);
	BOOL OnListLButtonUp(UINT nFlags, CPoint point);
	BOOL OnListLButtonDblClk(UINT nFlags, CPoint point);
	BOOL OnListMouseMove(UINT nFlags, CPoint point);

	GM_ITEMSTATE GetItemState(int nItem) const;
	GM_ITEMSTATE GetItemState(HTREEITEM hti) const;
	void RecalcListColumnsToFit();

	// Local methods
	void DrawTreeItem(CDC* pDC, HTREEITEM hti, const GANTTITEM& gi, BOOL bSelected, COLORREF crBack = CLR_NONE);
	void DrawTreeItemText(CDC* pDC, HTREEITEM hti, int nCol, const GANTTITEM& gi, BOOL bSelected, COLORREF crBack = CLR_NONE);
	COLORREF DrawTreeItemBackground(CDC* pDC, HTREEITEM hti, const GANTTITEM& gi, const CRect& rItem, const CRect& rClient, BOOL bSelected);
	void DrawListHeaderItem(CDC* pDC, int nCol);
	void DrawListHeaderRect(CDC* pDC, const CRect& rItem, const CString& sItem, CThemed* pTheme);
	
	void DrawListItem(CDC* pDC, int nItem, const GANTTITEM& gi, BOOL bSelected);
	BOOL DrawListItemColumn(CDC* pDC, int nItem, int nCol, const GANTTITEM& gi, BOOL bSelected, BOOL bRollup);
	void DrawListItemText(CDC* pDC, const GANTTITEM& gi, const CRect& rItem, const CRect& rClip, COLORREF crRow);
	void DrawListItemRollupText(CDC* pDC, HTREEITEM hti, const CRect& rItem, const CRect& rClip, COLORREF crRow);
	BOOL DrawListItemColumnRect(CDC* pDC, int nCol, const CRect& rColumn, const GANTTITEM& gi, BOOL bSelected, BOOL bRollup);
	void DrawListItemRollup(CDC* pDC, HTREEITEM htiParent, int nCol, const CRect& rColumn, BOOL bSelected);

	void DrawListItemYears(CDC* pDC, const CRect& rItem, int nYear, int nNumYears,
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday);
	void DrawListItemYear(CDC* pDC, const CRect& rYear, int nYear, 
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday);
	void DrawListItemMonths(CDC* pDC, const CRect& rItem, int nMonth, int nNumMonths, int nYear, 
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday);
	void DrawListItemMonth(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, 
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday);
	void DrawListItemWeeks(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, 
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday);
	void DrawListItemDays(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, 
							const GANTTITEM& gi, BOOL bSelected, BOOL bRollup, BOOL& bToday, BOOL bDrawHours);

	void DrawGanttBar(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi);
	void DrawGanttDone(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi);
	void DrawGanttMilestone(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi);

	BOOL DrawWeekend(CDC* pDC, const COleDateTime& dtDay, const CRect& rDay);
	BOOL DrawToday(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, BOOL bSelected);
	void DrawGanttParentEnds(CDC* pDC, const GANTTITEM& gi, const CRect& rBar, 
							 const COleDateTime& dtMonthStart, const COleDateTime& dtMonthEnd);

	enum DIV_TYPE { DIV_NONE = -1, DIV_VERT_LIGHT, DIV_VERT_MID, DIV_VERT_DARK, DIV_HORZ };

	void DrawItemDivider(CDC* pDC, const CRect& rItem, DIV_TYPE nType, BOOL bSelected);
	DIV_TYPE GetVerticalDivider(int nMonth, int nYear) const;

	void ClearDependencyPickLine(CDC* pDC = NULL);
	BOOL DrawDependencyPickLine(const CPoint& ptClient);
	void EndDependencyEdit();

	void BuildListColumns();
	void UpdateListColumns(int nWidth = -1);
	void RecalcListColumnWidths(int nFromWidth, int nToWidth);
	void UpdateListColumnsWidthAndText(int nWidth = -1);
	CString FormatListColumnHeaderText(GTLC_MONTH_DISPLAY nDisplay, int nMonth, int nYear) const;

	BOOL GetTreeItemRect(HTREEITEM hti, int nCol, CRect& rItem, BOOL bText = FALSE) const;
	HFONT GetTreeItemFont(HTREEITEM hti, const GANTTITEM& gi, GTLC_COLUMN nColID);
	BOOL GetListColumnDate(int nCol, int& nMonth, int& nYear) const;
	BOOL GetListColumnDate(int nCol, COleDateTime& date, BOOL bEndOfMonth = FALSE) const;
	void CalcMinMonthWidths();
	int GetMinMonthWidth(GTLC_MONTH_DISPLAY nDisplay) const;
	void BuildTreeColumns();
	GTLC_MONTH_DISPLAY GetColumnDisplay(int nColWidth);
	int GetColumnWidth() const;
	double GetMonthWidth(int nColWidth) const;
	int GetRequiredListColumnCount() const;
	int GetRequiredListColumnCount(GTLC_MONTH_DISPLAY nDisplay) const;
	BOOL ZoomTo(GTLC_MONTH_DISPLAY nNewDisplay, int nNewMonthWidth);
	void DeleteItem(HTREEITEM hti);
	void RemoveDeletedTasks(HTREEITEM hti, const ITASKLISTBASE* pTasks, const CDWordSet& mapIDs);
	int FindColumn(int nScrollPos) const;
	int FindColumn(int nMonth, int nYear) const;
	int FindColumn(const COleDateTime& date) const;
	BOOL GetDateFromScrollPos(int nScrollPos, COleDateTime& date) const;
	BOOL GetScrollPosFromDate(const COleDateTime& date, int& nPos) const;
	BOOL GetDrawPosFromDate(const COleDateTime& date, int& nPos) const;
	void ScrollTo(const COleDateTime& date);
	void InitItemHeights();
	int GetStartYear(GTLC_MONTH_DISPLAY nDisplay) const;
	int GetEndYear(GTLC_MONTH_DISPLAY nDisplay) const;
	COleDateTime GetStartDate(GTLC_MONTH_DISPLAY nDisplay) const;
	COleDateTime GetEndDate(GTLC_MONTH_DISPLAY nDisplay) const;
	int GetNumMonths(GTLC_MONTH_DISPLAY nDisplay) const;
	BOOL ValidateMonthDisplay(GTLC_MONTH_DISPLAY& nDisplay, int& nWidth) const;
	BOOL CanSetMonthDisplay(GTLC_MONTH_DISPLAY nDisplay, int nWidth) const;
	void RecalcDateRange();
	BOOL GetListItemRect(int nItem, CRect& rItem) const;
	void IncrementItemPositions(HTREEITEM htiParent, int nFromPos);
	HIMAGELIST GetTaskIcon(DWORD dwTaskID, int& iImageIndex) const;

	GANTTITEM* GetGanttItem(DWORD dwTaskID, BOOL bCopyRefID = TRUE) const;
	BOOL RestoreGanttItem(const GANTTITEM& giPrev);

	DWORD TreeHitTestTask(const CPoint& point, BOOL bScreen) const;
	DWORD ListHitTestTask(const CPoint& point, BOOL bScreen, GTLC_HITTEST& nHit, BOOL bDragging) const;
	DWORD ListDependsHitTest(const CPoint& ptClient, DWORD& dwToTaskID);
	BOOL SelectTask(HTREEITEM hti, const IUISELECTTASK& select, BOOL bForwards);

	DWORD GetTaskID(HTREEITEM hti) const;
	DWORD GetTaskID(int nItem) const;
	GTLC_COLUMN GetTreeColumnID(int nCol) const;

	BOOL StartDragging(const CPoint& ptCursor);
	BOOL EndDragging(const CPoint& ptCursor);
	BOOL UpdateDragging(const CPoint& ptCursor);
	BOOL ValidateDragPoint(CPoint& ptDrag) const;
	BOOL IsValidDragPoint(const CPoint& ptDrag) const;
	void CancelDrag(BOOL bReleaseCapture);
	BOOL IsDragging() const;
	void GetDragLimits(CRect& rLimits) const;
	BOOL GetValidDragDate(const CPoint& ptCursor, COleDateTime& dtDrag) const;
	BOOL GetDateFromPoint(const CPoint& ptCursor, COleDateTime& date) const;
	COleDateTime GetNearestDate(const COleDateTime& date) const;
	double CalcMinDragDuration() const;
	BOOL CanDragTask(DWORD dwTaskID, GTLC_DRAG nDrag = GTLCD_ANY) const;
	BOOL SetListTaskCursor(DWORD dwTaskID, GTLC_HITTEST nHit) const;

	BOOL NotifyParentDateChange(GTLC_DRAG nDrag);
	void NotifyParentDragChange();

	BOOL UpdateTreeColumnWidths(CDC* pDC, UPDATECOLWIDTHACTION nAction);
	int CalcTreeColumnWidth(int nCol, CDC* pDC) const;
	CString GetLongestVisibleAllocTo(HTREEITEM hti) const;
	void RefreshItemBoldState(HTREEITEM hti = NULL, BOOL bAndChildren = TRUE);
	CString FormatDate(const COleDateTime& date, DWORD dwFlags = 0) const;

	BOOL CalcMilestoneRect(const GANTTITEM& gi, const CRect& rMonth, CRect& rMilestone) const;
	int GetBestTextPos(const GANTTITEM& gi, const CRect& rMonth) const;
	CString GetTreeItemColumnText(const GANTTITEM& gi, GTLC_COLUMN nColID) const;
	void GetGanttBarColors(const GANTTITEM& gi, COLORREF& crBorder, COLORREF& crFill) const;
	COLORREF GetTreeTextColor(const GANTTITEM& gi, BOOL bSelected, BOOL bLighter = FALSE) const;
	COLORREF GetTreeTextBkColor(const GANTTITEM& gi, BOOL bSelected, BOOL bAlternate) const;
	BOOL GetTaskStartEndDates(const GANTTITEM& gi, COleDateTime& dtStart, COleDateTime& dtDue) const;

	void RebuildTree(const ITASKLISTBASE* pTasks);
	void BuildTreeItem(const ITASKLISTBASE* pTasks, HTASKITEM hTask, HTREEITEM htiParent, BOOL bAndSiblings, BOOL bInsertAtEnd = TRUE);
	BOOL UpdateTask(const ITASKLISTBASE* pTasks, HTASKITEM hTask, IUI_UPDATETYPE nUpdate, BOOL bAndSiblings);
	void RecalcParentDates();
	void RecalcParentDates(HTREEITEM htiParent, GANTTITEM*& pGI);
	void UpdateParentStatus(const ITASKLISTBASE* pTasks, HTASKITEM hTask, BOOL bAndSiblings);
	void UpdateParentStatus(DWORD dwParentID);

	BOOL EditWantsResort(const ITASKLISTBASE* pTasks, IUI_UPDATETYPE nUpdate) const;
	void Sort(GTLC_COLUMN nBy, BOOL bAllowToggle, BOOL bAscending, BOOL bNotifyParent);
	int CompareTasks(DWORD dwTaskID1, DWORD dwTaskID2, const GANTTSORTCOLUMN& col) const;

	BOOL CalcDependencyEndPos(DWORD dwTaskID, GANTTDEPENDENCY& depend, BOOL bTo, LPPOINT lpp = NULL) const;
	BOOL BuildDependency(DWORD dwFromTaskID, DWORD, GANTTDEPENDENCY& depend) const;
	int BuildVisibleDependencyList(CGanttDependArray& aDepends) const;
	int BuildVisibleDependencyList(HTREEITEM htiFrom, CGanttDependArray& aDepends) const;
	BOOL IsDependencyPickLinePosValid() const;
	void ResetDependencyPickLinePos();

	BOOL IsDependencyEditing() const;
	BOOL IsPickingDependencyFromTask() const;
	BOOL IsPickingFromDependency() const;
	BOOL IsPickingDependencyToTask() const;
	BOOL IsDependencyEditingCancelled() const;
	BOOL IsDependencyEditingComplete() const;

	static int CALLBACK MultiSortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	
	static BOOL CalcDateRect(const CRect& rMonth, int nMonth, int nYear, 
							const COleDateTime& dtFrom, const COleDateTime& dtTo, CRect& rDate);
	static BOOL CalcDateRect(const CRect& rMonth, int nDaysInMonth, 
							const COleDateTime& dtMonthStart, const COleDateTime& dtMonthEnd, 
							const COleDateTime& dtFrom, const COleDateTime& dtTo, CRect& rDate);
	static CString GetTaskAllocTo(const ITASKLISTBASE* pTasks, HTASKITEM hTask);
	static BOOL CalcMinDragDuration(GTLC_SNAPMODE nMode, double& dMin);
	static void BuildTaskMap(const ITASKLISTBASE* pTasks, HTASKITEM hTask, CDWordSet& mapIDs, BOOL bAndSiblings);
	static BOOL DragDatesDiffer(const GANTTITEM& gi1, const GANTTITEM& gi2);
	static int GetColumnWidth(GTLC_MONTH_DISPLAY nDisplay, int nMonthWidth);
	static double GetMonthWidth(GTLC_MONTH_DISPLAY nDisplay, int nColWidth);
	static BOOL GetDateFromScrollPos(int nScrollPos, GTLC_MONTH_DISPLAY nDisplay, int nMonth, int nYear, const CRect& rColumn, COleDateTime& date);
	static BOOL IsVerticalDivider(DIV_TYPE nType);
	static BOOL IsDragging(GTLC_DRAG nDrag);
	static BOOL IsDraggingEnds(GTLC_DRAG nDrag);

private:
	void PreFixVScrollSyncBug();
	BOOL CalcDependencyEndPos(DWORD dwTaskID, int nItem, GANTTDEPENDENCY& depend, BOOL bTo, LPPOINT lpp) const;
	const GANTTDATERANGE& ActiveDateRange() const;

};

#endif // !defined(AFX_GANTTTREELIST_H__016B94F3_1D28_4532_97EF_95F1D9D5CE55__INCLUDED_)
