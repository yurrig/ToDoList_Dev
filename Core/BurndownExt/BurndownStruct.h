#if !defined(AFX_BURNDOWNSTRUCT_H__F2F5ABDC_CDB2_4197_A8E1_6FE134F95A20__INCLUDED_)
#define AFX_BURNDOWNSTRUCT_H__F2F5ABDC_CDB2_4197_A8E1_6FE134F95A20__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BurndownEnum.h"

#include "..\Shared\mapex.h"
#include "..\shared\timehelper.h"
#include "..\shared\datehelper.h"

#include "..\Interfaces\IEnums.h"

#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////

struct DISPLAYITEM
{
	UINT nYAxisID;
	BURNDOWN_CHARTTYPE nDisplay;
};

/////////////////////////////////////////////////////////////////////////////

struct STATSITEM 
{ 
	STATSITEM();
	virtual ~STATSITEM();

	BOOL HasStart() const;
	BOOL IsDone() const;
	
	void MinMax(COleDateTimeRange& dtExtents) const;

	double CalcTimeSpentInDays(const COleDateTime& date) const;
	double CalcTimeEstimateInDays() const;
	
	COleDateTime dtStart, dtDone; 
	double dTimeEst, dTimeSpent;
	TDC_UNITS nTimeEstUnits, nTimeSpentUnits;
	DWORD dwTaskID;

protected:
	static void MinMax(const COleDateTime& date, COleDateTimeRange& dtExtents);
	static double CalcTimeInDays(double dTime, TDC_UNITS nUnits);
	static TH_UNITS MapUnitsToTHUnits(TDC_UNITS nUnits);

};

/////////////////////////////////////////////////////////////////////////////

class CStatsItemArray : protected CArray<STATSITEM*, STATSITEM*>
{
public:
	CStatsItemArray();
	virtual ~CStatsItemArray();

	STATSITEM* AddItem(DWORD dwTaskID);
	STATSITEM* GetItem(DWORD dwTaskID) const;
	BOOL HasItem(DWORD dwTaskID) const;
	BOOL IsEmpty() const;
	int GetSize() const;

	void RemoveAll();
	void RemoveAt(int nIndex, int nCount = 1);

	void Sort();
	BOOL IsSorted() const;

	double CalcTimeSpentInDays(const COleDateTime& date) const;
	double CalcTotalTimeEstimateInDays() const;
	int CalculateIncompleteTaskCount(const COleDateTime& date, int nItemFrom, int& nNextItemFrom) const;

	void GetDataExtents(COleDateTimeRange& dtExtents) const;

	STATSITEM* operator[](int nIndex) const;

protected:
	CDWordSet m_setTaskIDs;
	
protected:
	int FindItem(DWORD dwTaskID) const;

	static int CompareItems(const void* pV1, const void* pV2);
};

#endif // !defined(AFX_BURNDOWNSTRUCT_H__F2F5ABDC_CDB2_4197_A8E1_6FE134F95A20__INCLUDED_)
