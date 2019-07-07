// DateHelper.h: interface for the CDateHelper class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WORKINGWEEK_H__2A4E63F6_A106_4295_BCBA_06D03CD67AE7__INCLUDED_)
#define AFX_WORKINGWEEK_H__2A4E63F6_A106_4295_BCBA_06D03CD67AE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//////////////////////////////////////////////////////////////////////

#include "DateHelper.h"

//////////////////////////////////////////////////////////////////////

class CWorkingDay
{
public:
	CWorkingDay(); // uses static initialisation set up by CWorkingWeek

	CWorkingDay(double dWorkingLengthInHours);	// eg.8

	CWorkingDay(double dWorkingLengthInHours,	// eg.8
				double dStartOfDayInHours,		// eg.9
				double dStartOfLunchInHours,	// eg.12
				double dEndOfLunchInHours);		// eg.13

	CWorkingDay(const CWorkingDay& workDay);

	static BOOL Initialise(double dWorkingLengthInHours,
						   double dStartOfDayInHours,		
						   double dStartOfLunchInHours,
						   double dEndOfLunchInHours);

	static BOOL IsValid(double dWorkingLengthInHours,
						double dStartOfDayInHours,
						double dStartOfLunchInHours,
						double dEndOfLunchInHours);

	double GetStartOfDayInHours() const;
	double GetEndOfDayInHours() const;
	double GetStartOfLunchInHours() const;
	double GetEndOfLunchInHours() const;
	double GetMiddleOfDayInHours() const;

	COleDateTime GetStartOfDay(const COleDateTime& date) const;
	COleDateTime GetEndOfDay(const COleDateTime& date) const;
	COleDateTime GetStartOfLunch(const COleDateTime& date) const;
	COleDateTime GetEndOfLunch(const COleDateTime& date) const;
	COleDateTime GetMiddleOfDay(const COleDateTime& date) const;

	double CalculateDurationInHours(double fromHour, double toHour) const;
	double GetLengthInHours(bool bIncludingLunch = false) const;
	double GetLunchLengthInHours() const;

	static double GetTimeOfDayInHours(const COleDateTime& date);
	
protected:
	double m_dStartOfDayInHours;
	double m_dWorkingLengthInHours;
	double m_dStartOfLunchInHours;
	double m_dEndOfLunchInHours;

};

//////////////////////////////////////////////////////////////////////

class CWeekend
{
public:
	CWeekend();	// uses static initialisation set up by CWorkingWeek
	CWeekend(DWORD dwDays); // eg. WD_SATURDAY | WD_SUNDAY
	CWeekend(const CWeekend& weekend);

	static BOOL Initialise(DWORD dwDays);
	static BOOL IsValid(DWORD dwDays);

	BOOL IsWeekend(DH_DAYOFWEEK nDOW) const;
	BOOL IsWeekend(OLE_DAYOFWEEK nDOW) const;
	BOOL IsWeekend(const COleDateTime& date) const;
	BOOL IsWeekend(double dDate) const;

	DWORD GetDays() const { return m_dwDays; }
	int GetLengthInDays() const;

protected:
	DWORD m_dwDays;
	int m_nLength;

	static int CalcLength(DWORD dwDays);
};

//////////////////////////////////////////////////////////////////////

class CWorkingWeek
{
public:
	CWorkingWeek(); // uses static initialisation

	CWorkingWeek(DWORD dwWeekendDays,			// eg. (DHW_SATURDAY | DHW_SUNDAY),
				 double dWorkingLengthInHours);	// eg. 8

	CWorkingWeek(DWORD dwWeekendDays,			// eg. (DHW_SATURDAY | DHW_SUNDAY),
				 double dWorkingLengthInHours,	// eg. 8
				 double dStartOfDayInHours,		// eg. 9
				 double dStartOfLunchInHours,	// eg. 12
				 double dEndOfLunchInHours);	// eg. 13

	CWorkingWeek(const CWorkingWeek& week);

	static BOOL Initialise(DWORD dwWeekendDays,				// eg. (DHW_SATURDAY | DHW_SUNDAY),
						   double dWorkingLengthInHours);	// eg. 8

	static BOOL Initialise(DWORD dwWeekendDays,				// eg. (DHW_SATURDAY | DHW_SUNDAY),
						   double dWorkingLengthInHours,	// eg. 8
						   double dStartOfDayInHours,		// eg. 9
						   double dStartOfLunchInHours,		// eg. 12
						   double dEndOfLunchInHours);		// eg. 13

	double CalculateDurationInHours(const COleDateTime& dtFrom, const COleDateTime& dtTo);
	double CalculateDurationInDays(const COleDateTime& dtFrom, const COleDateTime& dtTo);
	double CalculateDurationInWeeks(const COleDateTime& dtFrom, const COleDateTime& dtTo);

	BOOL MakeWeekday(COleDateTime& date, BOOL bForwards = TRUE, BOOL bTruncateTime = TRUE) const;
	COleDateTime ToWeekday(const COleDateTime& date, BOOL bForwards = TRUE) const;

	BOOL HasWeekend() const;
	DWORD GetWorkingDays() const;

	int GetLengthInDays() const;
	double GetLengthInHours(bool bIncludingLunch = false) const;

	double GetLengthInDaysAsRatio() const; // length in days / 7.0
	double GetLengthInHoursAsRatio(bool bIncludingLunch = false) const; // length in hours / 7.0 * 24

	const CWorkingDay& WorkDay() const { return m_WorkDay;	}
	const CWeekend& Weekend() const { return m_Weekend; }

protected:
	CWorkingDay m_WorkDay;
	CWeekend m_Weekend;
};

//////////////////////////////////////////////////////////////////////

class CTwentyFourSevenWeek : public CWorkingWeek
{
public:
	CTwentyFourSevenWeek() 
		: 
		CWorkingWeek(0,			// no weekend  
					 0,			// 12 am
					 24,		// 24 hours
					 12, 12)	// no lunch
	{
	}

};



//////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_WORKINGWEEK_H__2A4E63F6_A106_4295_BCBA_06D03CD67AE7__INCLUDED_)
