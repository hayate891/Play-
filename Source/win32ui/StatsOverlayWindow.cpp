#include "StatsOverlayWindow.h"
#include "win32/DefaultWndClass.h"
#include "win32/ClientDeviceContext.h"
#include "win32/MemoryDeviceContext.h"
#include "win32/Font.h"
#include "string_format.h"
#include "string_cast.h"
#include "WinUtils.h"

CStatsOverlayWindow::CStatsOverlayWindow()
{
	
}

CStatsOverlayWindow::CStatsOverlayWindow(HWND parentWnd)
: m_font(Framework::Win32::CreateFont(_T("Courier New"), 11))
{
	//Fill in render metrics
	{
		auto fontSize = Framework::Win32::GetFixedFontSize(m_font);
		m_renderMetrics.marginX = WinUtils::PointsToPixels(10);
		m_renderMetrics.marginY = WinUtils::PointsToPixels(10);
		m_renderMetrics.fontSizeX = fontSize.cx;
		m_renderMetrics.fontSizeY = fontSize.cy;
		m_renderMetrics.spaceY = WinUtils::PointsToPixels(3);
	}

	Create(WS_EX_TRANSPARENT | WS_EX_LAYERED, Framework::Win32::CDefaultWndClass::GetName(), _T(""), WS_POPUP, Framework::Win32::CRect(0, 0, 128, 128), parentWnd, NULL);
	SetClassPtr();
}

CStatsOverlayWindow::~CStatsOverlayWindow()
{

}

CStatsOverlayWindow& CStatsOverlayWindow::operator =(CStatsOverlayWindow&& rhs)
{
	CWindow::Reset();
	CWindow::MoveFrom(std::move(rhs));
	m_font = std::move(rhs.m_font);
	m_renderMetrics = std::move(rhs.m_renderMetrics);
	return (*this);
}

void CStatsOverlayWindow::Update(unsigned int frames)
{
	auto windowRect = GetWindowRect();

	auto screenDc = Framework::Win32::CClientDeviceContext(NULL);
	auto memDc = Framework::Win32::CMemoryDeviceContext(screenDc);
	auto memBitmap = Framework::Win32::CBitmap(CreateCompatibleBitmap(screenDc, windowRect.Width(), windowRect.Height()));
	memDc.SelectObject(memBitmap);
	memDc.SelectObject(m_font);
	SetTextColor(memDc, RGB(0xFF, 0xFF, 0xFF));
	SetBkColor(memDc, RGB(0, 0, 0));

	{
		std::lock_guard<std::mutex> profileZonesLock(m_profilerZonesMutex);

		uint64 totalTime = 0;

		for(const auto& zonePair : m_profilerZones)
		{
			totalTime += zonePair.second;
		}

		//float avgFrameTime = (static_cast<double>(totalTime) /  static_cast<double>(m_frames * 1000));
		//profilerTextResult = string_format(_T("Avg Frame Time: %0.2fms"), avgFrameTime);

		int x = m_renderMetrics.marginX;
		int y = m_renderMetrics.marginY;
		for(const auto& zonePair : m_profilerZones)
		{
			float ratioSpent = (totalTime != 0) ? static_cast<double>(zonePair.second) / static_cast<double>(totalTime) : 0;
			float msSpent = (frames != 0) ? static_cast<double>(zonePair.second) / static_cast<double>(frames * 1000) : 0;
			memDc.TextOut(x + 0  , y, string_cast<std::tstring>(zonePair.first).c_str());
			memDc.TextOut(x + (m_renderMetrics.fontSizeX * 10), y, string_format(_T("%0.2f%%"), ratioSpent * 100.f).c_str());
			memDc.TextOut(x + (m_renderMetrics.fontSizeX * 20), y, string_format(_T("%0.2fms"), msSpent).c_str());
			y += m_renderMetrics.fontSizeY + m_renderMetrics.spaceY;
		}

		for(auto& zonePair : m_profilerZones) { zonePair.second = 0; }
	}

	POINT dstPt = { windowRect.Left(), windowRect.Top() };
	SIZE dstSize = { windowRect.Width(), windowRect.Height() };
	POINT srcPt = { 0, 0 };

	BOOL result = UpdateLayeredWindow(m_hWnd, screenDc, &dstPt, &dstSize, memDc, &srcPt, RGB(0, 0, 0), nullptr, ULW_COLORKEY);
	assert(result == TRUE);
}

void CStatsOverlayWindow::OnProfileFrameDone(const CProfiler::ZoneArray& zones)
{
	std::lock_guard<std::mutex> profileZonesLock(m_profilerZonesMutex);

	for(auto& zone : zones)
	{
		auto zoneIterator = m_profilerZones.find(zone.name);
		if(zoneIterator != std::end(m_profilerZones))
		{
			zoneIterator->second += zone.totalTime;
		}
		else
		{
			m_profilerZones.insert(std::make_pair(zone.name, zone.totalTime));
		}
	}
}