#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <queue>
#include <assert.h>
// 그리기
//  멈충 상태에서 마우스 클릭 드래그
//  브러쉬 크기 1~9
#pragma pack(push, 4)
struct TSnowUnit{
    float x, y;

    UINT16 life;
    char vectorX; // per 0.1
    char vectorY; // per 0.1

    byte power;
    byte Immune_Wind;
    byte cntCollison; //통계 충돌 카운팅
    char maxWave;
};
struct TPixel{
    BYTE snow_unit;
    BYTE snow_color;

    INT16 at;// attach point
    // 허공(확장가능) -1
    // const short gc_AT_Ground = 30;;
    // const short gc_AT_Object = 20;
    // const short gc_AT_Snow = 10;
    void AttachSnowUnit()
    {
        if(255 != snow_unit)
            snow_unit++;
    }
    BOOL DetachSnowUnit()
    {
        if(!snow_unit)
            return FALSE;

        snow_unit--;
        return TRUE;
    }
    BYTE GetSnowColor()
    {
        return snow_color;
    }
    void SetSnowColor(BYTE c)
    {
        snow_color = c;
    }
    void IncrementSnowColor(BYTE c)
    {
        byte prev = snow_color;
        snow_color += c;
        if(snow_color < prev)
            snow_color = 255;
    }
    BOOL Query_Is_Full()
    {
        if(255 <= (UINT32)snow_color + (UINT32)(snow_unit*85))
            return TRUE;
        else
            return FALSE;
    }
};
#pragma pack(pop)
struct TPixelColor;
namespace DATA{
    enum struct E_MODE : INT32{
        E_Default = 0,
        E_Simulator = 0,
        E_Benchmark = 1,
        E_Game_Poop = 2,
    };
    E_MODE g_flagMode = E_MODE::E_Simulator;

    const LONGLONG gc_Benchmark_TimeConsumptionSec = 2*60 + 0;
    LONGLONG g_Benchmark_TimeRemaining = 0;
    LONGLONG g_Benchmark_Tick_per_Sec = 0;
    UINT64 g_Benchmark_Score = 0;
    UINT64 g_Benchmark_Score_Best = 0;

    double g_PoopGame_Score = 0;
    UINT64 g_PoopGame_Score_Best = 0;

    BOOL g_flagPause = FALSE;
    int g_flagViewMode = 0;
    const int gc_MaxViewMode = 3;
    // g_flagViewMode
    //  0 일반
    //  1 눈 동적인
    //  2 눈 정적인
    //  3 충돌맵

    const int gc_ScreenSizeWidth = 1024;
    const int gc_ScreenSizeHeight = 768;
    const double gc_fps = 30.;
    const RECT gc_rect = {0, 0, gc_ScreenSizeWidth, gc_ScreenSizeHeight};
    const int gc_maxHorizonWave = 1;
    const int gc_minVerticalSpeed = 2; // byte
    const int gc_maxVerticalSpeed = 3; // byte
    const int gc_maxWindSpeed = 10;

    // 바닥 / 오브젝트에 눈이 쌓이는 최대 높이
    const short gc_AT_Ground = 30;;
    const short gc_AT_Object = 20;
    const short gc_AT_Snow = 10;

    // 쌓이는 눈의 밀집도
    // 수치가 높으면 천천히 견고하게 쌓인다
    const UINT32 gc_magicCODE = 128;

    const int gc_StartPos__Top = 128;

    //----------------------------------------------------------------

    struct TPoopGamePlayer{
        TPoopGamePlayer()
        {
            Initialize();
        }
        void Initialize()
        {
            fX = gc_ScreenSizeWidth / 2.f;
            fY = gc_ScreenSizeHeight / 2.f;
            fSize = (float)Setting.sc_Size_MinRadius;
        }
        float fX, fY;
        float fSize;

        struct{
            static const int sc_Speed_per_Sec = 50;
            static const int sc_LimitFPS = 30;

            static const int sc_Size_MinRadius = 3;
            static const int sc_Size_MaxRadius = 30;

            static const int sc_Recduction_per_Sec__Numerator = 1;
            static const int sc_Recduction_per_Sec__Denominator = 10;
        }Setting;
    }g_PoopGamePlayer;

    //----------------------------------------------------------------
    static_assert(gc_minVerticalSpeed < gc_maxVerticalSpeed, "");
    static_assert(gc_StartPos__Top >= 50, "");
    static_assert(0 < gc_maxHorizonWave && gc_maxHorizonWave < 16, "");
    static_assert(MAXCHAR >= gc_maxWindSpeed*10, "");

    //----------------------------------------------------------------
    int g_iSel_HorizonWaveStep = 0;

    const UINT16 gc_minLife = gc_StartPos__Top / gc_minVerticalSpeed;
    const byte gc_minSizeBigSnow = 200;

    // 최대 눈덩이 수는 화면픽셀수의 50%
    // 640 * 480 기준 240KB
    const int gc_nSnowUnits = gc_ScreenSizeWidth * gc_ScreenSizeHeight / 2;
    TSnowUnit g_SnowUnits[gc_nSnowUnits] = {0};

    // 최소 틱틱 1%
    const int gc_iStepUsingSnowUnits = gc_nSnowUnits / 100;
    static_assert(0 < gc_iStepUsingSnowUnits, "");
    
    // 초기 설정
    int g_nUsingSnowUnits = gc_iStepUsingSnowUnits;
    int g_iLastLiveUnitIndex = g_nUsingSnowUnits - 1;

    // 아래로 내려가기 때문에 일반적 [y][x] 가 아닌 [x][y]로 사용한다....
    const int gc_MapSizeWidth = gc_ScreenSizeWidth+(gc_maxHorizonWave*2);
    const int gc_MapSizeHeight = gc_ScreenSizeHeight+gc_maxVerticalSpeed;
    TPixel g_Map[gc_MapSizeWidth][gc_MapSizeHeight] = {0};

    const int g_maxSnowVectorDistance = max(gc_maxWindSpeed+gc_maxVerticalSpeed, gc_maxHorizonWave);
    const float g_fmaxSnowVectorDistance = static_cast<float>(g_maxSnowVectorDistance);
    const int g_minWallThickness = g_maxSnowVectorDistance;

    const int gc_minBrushSize = g_minWallThickness / 2 +2;
    int g_BrushSize = gc_minBrushSize;

    HWND g_hwnd;
    HDC g_hdc;

    HDC g_hdcB;
    HBITMAP g_hbitmapB;
    TPixelColor* g_pBitmapB;

    HDC g_hdcBG;
    HBITMAP g_hbitmapBG;
    TPixelColor* g_pBitmapBG;

    DWORD g_Speed = 1;
    int g_Wind = 0;

    BOOL g_flagAutoEnvironment = TRUE;
    int g_AutoWind_NextTick = 64;
    int g_AutoWind_ModifyPower = 0;
    float g_AutoSnowIncrement_NextTime_Remaining = 0.f;

    BOOL g_flagShowInfoText = TRUE;

    int g_stats_FPS_Counting = 0;
    LONGLONG g_stats_FPS_Sum = 0;
    float g_stats_FPS = 0.f;

    INT64 g_stats_CountingFrame_per_This_Second = 0;

    INT64 g_stats_SnowUnitActive_Counting = 0;
    INT64 g_stats_SnowUnitDraw_Counting = 0;
    INT64 g_stats_SnowUnitCollision_Counting = 0;
    INT64 g_stats_SnowUnitStack_Counting = 0;
    INT64 g_stats_SnowUnitCollapse_Counting = 0;  // 붕괴

    INT64 g_stats_SnowUnitActive_Average = 0;
    INT64 g_stats_SnowUnitDraw_Average = 0;
    INT64 g_stats_SnowUnitCollision_Average = 0;
    INT64 g_stats_SnowUnitStack_Average = 0;
    INT64 g_stats_SnowUnitCollapse_Average = 0;  // 붕괴
}
struct TPixelColor{
    BYTE B;
    BYTE G;
    BYTE R;
    BYTE A;

    TPixelColor(BYTE c):A(0), R(c), G(c), B(c){}
    TPixelColor(BYTE r, BYTE g, BYTE b):A(0), R(r), G(g), B(b){}
    bool operator < (const TPixelColor& r) const
    {
        return *((UINT32*)this) < *((UINT32*)&r);
    }
    bool operator > (const TPixelColor& r) const
    {
        return *((UINT32*)this) > *((UINT32*)&r);
    }
    bool operator <= (const TPixelColor& r) const
    {
        return *((UINT32*)this) <= *((UINT32*)&r);
    }
    bool operator >= (const TPixelColor& r) const
    {
        return *((UINT32*)this) >= *((UINT32*)&r);
    }
    bool operator == (const TPixelColor& r) const
    {
        return *((UINT32*)this) == *((UINT32*)&r);
    }
    void operator = (COLORREF col)
    {
        R=GetRValue(col), G=GetGValue(col), B=GetBValue(col);
    }
};
TPixelColor GetPixel(TPixelColor* p, int x, int y)
{
    const size_t offset = (DATA::gc_ScreenSizeWidth * (DATA::gc_ScreenSizeHeight - y-1)) + x;
    return p[offset];
}
void SetPixel(TPixelColor* p, int x, int y, TPixelColor c)
{
    const size_t offset = (DATA::gc_ScreenSizeWidth * (DATA::gc_ScreenSizeHeight - y-1)) + x;
    p[offset] = c;
}
namespace GFN{
    using namespace DATA;
    
    void Initialize(HWND hwnd)
    {
        srand((UINT32)time(0));

        g_hwnd = hwnd;
        g_hdc = ::GetDC(hwnd);

        BITMAPINFO info;
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = DATA::gc_ScreenSizeWidth;
        info.bmiHeader.biHeight = DATA::gc_ScreenSizeHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        g_hdcB = CreateCompatibleDC(g_hdc);
        g_hbitmapB =::CreateDIBSection(g_hdcB, &info, DIB_RGB_COLORS, (void **)&g_pBitmapB, 0, 0);
        SelectObject( g_hdcB, g_hbitmapB );

        g_hdcBG = CreateCompatibleDC(g_hdc);
        g_hbitmapBG =::CreateDIBSection(g_hdcBG, &info, DIB_RGB_COLORS, (void **)&g_pBitmapBG, 0, 0);
        SelectObject( g_hdcBG, g_hbitmapBG );
    }
    void Shutdown()
    {
        DeleteDC(g_hdcB);
        DeleteObject(g_hbitmapB);
        DeleteDC(g_hdcBG);
        DeleteObject(g_hbitmapBG);
        ReleaseDC(g_hwnd, g_hdc);
    }
    int Second_to_HHMMSS(char* str, int len, LONGLONG sec)
    {
        LONGLONG h = sec / (60*60);
        LONGLONG m = (sec - (h*60*60)) / 60;
        LONGLONG s = sec % 60;
        return sprintf_s(str, len, "%02u:%02u:%02u", (UINT32)h, (UINT32)m, (UINT32)s);
    }
}
HINSTANCE hInst;
const WCHAR* szTitle=L"Snow Simulator";
const WCHAR* szWindowClass=L"Snow_Simulator__by_LastPenguin";

void UpdateStats();
void FrameProcess_PoopGame_Player();
BOOL UpdateFPS(const LONGLONG& elapsed, const LONGLONG& TickperSec)
{
    DATA::g_stats_FPS_Counting++;
    DATA::g_stats_FPS_Sum += elapsed;

    if(DATA::g_stats_FPS_Sum < TickperSec)
        return FALSE;

    const double s = TickperSec / (double)DATA::g_stats_FPS_Sum;
    DATA::g_stats_FPS = (float)(DATA::g_stats_FPS_Counting * s);
    if(DATA::g_stats_FPS < 0.0001f)
        DATA::g_stats_FPS = 0.f;

    DATA::g_stats_FPS_Counting = 0;
    DATA::g_stats_FPS_Sum = 0;
    
    return TRUE;
}
#include <chrono>
                                                // 이 코드 모듈에 들어 있는 함수의 정방향 선언입니다.
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void MainProcess(BOOL bPause);
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    MyRegisterClass(hInstance);

    // 응용 프로그램 초기화를 수행합니다.
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }


    MSG msg;

    // 기본 메시지 루프입니다.
    BOOL bQuit = FALSE;

    ::SetThreadAffinityMask(::GetCurrentThread(), 1);
    LARGE_INTEGER timeTick;
    ::QueryPerformanceFrequency(&timeTick);
    DATA::g_Benchmark_Tick_per_Sec = timeTick.QuadPart;

    LARGE_INTEGER timePrev;
    timePrev.QuadPart = 0;
    while(!bQuit)
    {
        if(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
        {
            if(msg.message==WM_QUIT)
            {
                bQuit=TRUE;
            }
            else if (!TranslateAccelerator(msg.hwnd, 0, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else if(DATA::g_flagMode == DATA::E_MODE::E_Benchmark)
        {
            LARGE_INTEGER timeNow;
            ::QueryPerformanceCounter(&timeNow);
            if(0 == DATA::g_Benchmark_Score)
            {
                timePrev.QuadPart = timeNow.QuadPart;
            }
            else
            {
                LONGLONG elapsed = timeNow.QuadPart - timePrev.QuadPart;
                
                if(UpdateFPS(elapsed, timeTick.QuadPart))
                    UpdateStats();
                DATA::g_Benchmark_TimeRemaining -= elapsed;

                timePrev.QuadPart = timeNow.QuadPart;
            }
            DATA::g_Benchmark_Score++;

            if(DATA::g_Benchmark_TimeRemaining <= 0)
            {
                DATA::g_Benchmark_TimeRemaining = 0;
                DATA::g_flagMode = DATA::E_MODE::E_Simulator;
            }

            DATA::g_stats_CountingFrame_per_This_Second++;
            MainProcess(FALSE);


            if(DATA::g_flagMode != DATA::E_MODE::E_Benchmark)
            {
                // 벤치마킹 종료
                DATA::g_flagPause = TRUE;
                if(DATA::g_Benchmark_Score_Best < DATA::g_Benchmark_Score)
                    DATA::g_Benchmark_Score_Best = DATA::g_Benchmark_Score;
            }
        }
        else if(DATA::g_flagPause)
        {
            MainProcess(TRUE);
            Sleep(1000/60);
        }
        else if(0 < DATA::g_Speed)
        {
            LARGE_INTEGER timeNow;
            ::QueryPerformanceCounter(&timeNow);

            LONGLONG t = (LONGLONG)(timeTick.QuadPart / (DATA::gc_fps*DATA::g_Speed));
            LONGLONG elapsed = timeNow.QuadPart - timePrev.QuadPart;
            if(elapsed < t)
                continue;

            if(UpdateFPS(elapsed, timeTick.QuadPart))
                UpdateStats();
            timePrev.QuadPart = timeNow.QuadPart;

            DATA::g_stats_CountingFrame_per_This_Second++;
            MainProcess(FALSE);
        }
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;//LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNOW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName   = 0;//MAKEINTRESOURCEW(IDC_SNOW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = 0;//LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    RECT rect = {0, 0, DATA::gc_ScreenSizeWidth, DATA::gc_ScreenSizeHeight};
    DWORD style = ::GetWindowLong(hWnd, GWL_STYLE);
    DWORD styleEx = ::GetWindowLong(hWnd, GWL_EXSTYLE);
    BOOL bMenu = GetMenu(hWnd)? TRUE : FALSE;
    ::AdjustWindowRectEx(&rect, style, bMenu, styleEx);
    ::SetWindowPos(hWnd, 0, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_NOMOVE);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void Draw_from_Mouse();
void ShowHelpWindow(HWND hWnd)
{
    // 방향기 wasd
    ::MessageBoxA(DATA::g_hwnd,
        "- shortcut key -\n"
        "Reset\n\tF5\n"
        "ViewMode Change\n\tV\n"
        "Pause\n\tSpacebar\n"
        "\n"
        "Mode (Automatic/Manual)\n\tEnter\n"
        "Snow (Increment/Decrement)\n\tArrow Up / Arrow Down\n"
        "Wind (Left/Right)\n\tArrow Left / Arrow Right\n\tKeypad Number\n"
        "Brush Size\n\t1 ~ 9\n"
        "FPS (Up / Down)\n\tPageUp / PageDown\n"
        "\n\n- Compile Time -\n\t"
        __DATE__
        "\n- Create By -\n\tlastpenguin83@gmail.com"
        , "Help"
        , MB_OK);
}
void Reset()
{
    ::FillRect(DATA::g_hdcBG, &DATA::gc_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ::FillRect(DATA::g_hdc, &DATA::gc_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    ::memset(DATA::g_Map, 0, sizeof(DATA::g_Map));
    ::memset(DATA::g_SnowUnits, 0, sizeof(DATA::g_SnowUnits));

    for(int i=0;i<DATA::gc_MapSizeWidth; i++)
    {
        for(int j=DATA::gc_ScreenSizeHeight; j<DATA::gc_MapSizeHeight;j++)
        {
            DATA::g_Map[i][j].snow_color = 255;
            DATA::g_Map[i][j].snow_unit = 255;
            DATA::g_Map[i][j].at = DATA::gc_AT_Ground;
        }
    }
    // 최하단 눈쌓임 가능 속성 ON
    for(int i=0;i<DATA::gc_MapSizeWidth; i++)
        DATA::g_Map[i][DATA::gc_ScreenSizeHeight-1].at = -1;


    DATA::g_Speed = 1;
    DATA::g_iSel_HorizonWaveStep = 0;

    // stats
    DATA::g_stats_FPS_Counting = 0;
    DATA::g_stats_FPS_Sum = 0;
    DATA::g_stats_FPS = 0.f;

    DATA::g_stats_CountingFrame_per_This_Second = 0;

    DATA::g_stats_SnowUnitActive_Counting = 0;
    DATA::g_stats_SnowUnitDraw_Counting = 0;
    DATA::g_stats_SnowUnitCollision_Counting = 0;
    DATA::g_stats_SnowUnitStack_Counting = 0;
    DATA::g_stats_SnowUnitCollapse_Counting = 0;

    DATA::g_stats_SnowUnitActive_Average = 0;
    DATA::g_stats_SnowUnitDraw_Average = 0;
    DATA::g_stats_SnowUnitCollision_Average = 0;
    DATA::g_stats_SnowUnitStack_Average = 0;
    DATA::g_stats_SnowUnitCollapse_Average = 0;
}
void MaxUnitsSet(int n)
{
    if(DATA::gc_nSnowUnits < n)
        n = DATA::gc_nSnowUnits;
    else if(n < DATA::gc_iStepUsingSnowUnits)
        n = DATA::gc_iStepUsingSnowUnits;

    DATA::g_nUsingSnowUnits = n;

    if(DATA::g_iLastLiveUnitIndex < DATA::g_nUsingSnowUnits)
        DATA::g_iLastLiveUnitIndex = DATA::g_nUsingSnowUnits -1;
}
void MaxUnitsIncrement()
{
    if(DATA::g_nUsingSnowUnits < DATA::gc_nSnowUnits)
    {
        DATA::g_nUsingSnowUnits += DATA::gc_iStepUsingSnowUnits;

        if(DATA::gc_nSnowUnits < DATA::g_nUsingSnowUnits)
            DATA::g_nUsingSnowUnits = DATA::gc_nSnowUnits;

        if(DATA::g_iLastLiveUnitIndex < DATA::g_nUsingSnowUnits)
            DATA::g_iLastLiveUnitIndex = DATA::g_nUsingSnowUnits -1;
    }
}
void MaxUnitsDecrement()
{
    if(DATA::gc_iStepUsingSnowUnits < DATA::g_nUsingSnowUnits)
    {
        DATA::g_nUsingSnowUnits -= DATA::gc_iStepUsingSnowUnits;

        if(DATA::g_nUsingSnowUnits < DATA::gc_iStepUsingSnowUnits)
            DATA::g_nUsingSnowUnits = DATA::gc_iStepUsingSnowUnits;
    }
}
void RequestBenchmark()
{
    char strHMS[64];
    GFN::Second_to_HHMMSS(strHMS, 64, DATA::gc_Benchmark_TimeConsumptionSec);

    char str[1024];
    sprintf_s(str, "Time Consumption %s\n"
        "※ Tese CPU CORE = index 0\n"
        "Continue ?"
        , strHMS);

    if(IDYES != ::MessageBoxA(DATA::g_hwnd, str, "Benchmark Test", MB_YESNO))
        return;

    DATA::g_flagMode = DATA::E_MODE::E_Benchmark;
    Reset();

    DATA::g_nUsingSnowUnits = DATA::gc_iStepUsingSnowUnits;
    DATA::g_iLastLiveUnitIndex = DATA::g_nUsingSnowUnits - 1;
    //DATA::g_Speed = 벤치마킹에서는 사용하지 않는다
    DATA::g_Wind = 0;

    DATA::g_flagAutoEnvironment = TRUE;
    DATA::g_AutoWind_NextTick = 64;
    DATA::g_AutoWind_ModifyPower = 0;
    DATA::g_AutoSnowIncrement_NextTime_Remaining = 0.f;
    DATA::g_flagShowInfoText = TRUE;

    srand(0);
    DATA::g_flagViewMode = 0;
    DATA::g_flagPause = FALSE;

    DATA::g_Benchmark_Score = 0;
    DATA::g_Benchmark_TimeRemaining = DATA::gc_Benchmark_TimeConsumptionSec * DATA::g_Benchmark_Tick_per_Sec;
}
void RequestPoopGame()
{
    if(IDYES != ::MessageBoxA(DATA::g_hwnd, "Continue?\nKey : WASD", "Poop Game", MB_YESNO))
        return;

    DATA::g_flagMode = DATA::E_MODE::E_Game_Poop;
    Reset();

    DATA::g_nUsingSnowUnits = DATA::gc_iStepUsingSnowUnits;
    DATA::g_iLastLiveUnitIndex = DATA::g_nUsingSnowUnits - 1;
    DATA::g_Speed = 1;
    DATA::g_Wind = 0;

    DATA::g_flagAutoEnvironment = TRUE;
    DATA::g_AutoWind_NextTick = 64;
    DATA::g_AutoWind_ModifyPower = 0;
    DATA::g_AutoSnowIncrement_NextTime_Remaining = 0.f;
    DATA::g_flagShowInfoText = TRUE;

    srand((UINT32)time(0));
    DATA::g_flagViewMode = 0;
    DATA::g_flagPause = FALSE;

    DATA::g_PoopGame_Score = 0;

    DATA::g_PoopGamePlayer.Initialize();
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        GFN::Initialize(hWnd);
        Reset();
        //ShowHelpWindow(hWnd);
        break;
        
    case WM_KEYDOWN:
        if(DATA::g_flagMode == DATA::E_MODE::E_Benchmark)
            return 0;

        switch(wParam)
        {
        case 0x41 + 'v'-'a':
            // v
            if(DATA::g_flagViewMode == DATA::gc_MaxViewMode)
                DATA::g_flagViewMode = 0;
            else
                DATA::g_flagViewMode++;
            break;
        case VK_SPACE:
            DATA::g_flagPause = !DATA::g_flagPause;
            break;

        case VK_F1:
            ShowHelpWindow(hWnd);
            break;
        case VK_F8:
            RequestBenchmark();
            break;
        case VK_F9:
            RequestPoopGame();
            break;
        case VK_F12:
            DATA::g_flagShowInfoText = !DATA::g_flagShowInfoText;
            break;

        case VK_F5:
            Reset();
            break;

        case VK_RETURN:
            DATA::g_flagAutoEnvironment = !DATA::g_flagAutoEnvironment;
            break;

        case VK_UP:
            if(DATA::g_flagMode == DATA::E_MODE::E_Game_Poop)
                break;
            MaxUnitsIncrement();
            break;
        case VK_DOWN:
            if(DATA::g_flagMode == DATA::E_MODE::E_Game_Poop)
                break;
            MaxUnitsDecrement();
            break;
        case VK_LEFT:
            if(DATA::g_flagMode == DATA::E_MODE::E_Game_Poop)
                break;
            if(-DATA::gc_maxWindSpeed < DATA::g_Wind)
                DATA::g_Wind--;
            break;
        case VK_RIGHT:
            if(DATA::g_flagMode == DATA::E_MODE::E_Game_Poop)
                break;
            if(DATA::g_Wind < DATA::gc_maxWindSpeed)
                DATA::g_Wind++;
            break;
        case VK_PRIOR: // 페이지업
            DATA::g_Speed++;
            if(32 < DATA::g_Speed)
                DATA::g_Speed = 32;
            break;
        case VK_NEXT: // 페이지 다운
            if(DATA::g_Speed > 1)
                DATA::g_Speed--;
            break;
        default:
            if(0x31 <= wParam && wParam <= 0x39)
            {
                DATA::g_BrushSize = DATA::gc_minBrushSize +(int)wParam - 0x31;
            }
            else if(VK_NUMPAD0 <= wParam && wParam <= VK_NUMPAD9)
            {
                if(0 == DATA::g_Wind)
                    DATA::g_Wind = (int)(wParam-VK_NUMPAD0);
                else
                    DATA::g_Wind = DATA::g_Wind / abs(DATA::g_Wind) * (int)(wParam-VK_NUMPAD0);
            }
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다.
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_ERASEBKGND:
        return 0;


    case WM_MOUSEMOVE:
        switch(DATA::g_flagMode)
        {
        case DATA::E_MODE::E_Simulator:
            Draw_from_Mouse();
            break;
        }
        break;

    case WM_DESTROY:

        GFN::Shutdown();
        PostQuitMessage(0);
        break;

        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
int GetRandPercent()
{
    return rand() % 1001;
}
int GetRandom_Vector_H()
{
    const int minHorizonSpeed = -(DATA::gc_maxHorizonWave*10);
    const int rndHorizonSpeed = (DATA::gc_maxHorizonWave * 2 * 10) + 1;
    return minHorizonSpeed + (rand() % (rndHorizonSpeed));
}
int GetRandom_Vector_V()
{
    const int minVerticalSpeed = DATA::gc_minVerticalSpeed*10;
    const int rndVerticalSpeed = (DATA::gc_maxVerticalSpeed - DATA::gc_minVerticalSpeed) * 10 + 1;
    return minVerticalSpeed + (rand() % rndVerticalSpeed);
}
void Recycle(TSnowUnit* p)
{
    const int TickV = (DATA::gc_StartPos__Top + DATA::gc_ScreenSizeHeight) * 2 / DATA::gc_maxVerticalSpeed / DATA::gc_minVerticalSpeed;
    int xMaxVecLtoR;
    int xMaxVecRtoL;
    if(0 < DATA::g_Wind)
    {
        xMaxVecRtoL = DATA::g_Wind;
        xMaxVecLtoR = DATA::gc_maxHorizonWave;
    }
    else if(DATA::g_Wind < 0)
    {
        xMaxVecRtoL = DATA::gc_maxHorizonWave;
        xMaxVecLtoR = abs(DATA::g_Wind);
    }
    else
    {
        xMaxVecLtoR = DATA::gc_maxHorizonWave;
        xMaxVecRtoL = DATA::gc_maxHorizonWave;
    }

    int xMin = -xMaxVecRtoL * TickV;
    int xMax = xMaxVecLtoR * TickV + DATA::gc_ScreenSizeWidth;
    const int xDistance = xMax - xMin + 1;
    p->x = (float)(xMin + rand()%xDistance);
    p->y = (float)(-(rand() % DATA::gc_StartPos__Top) - 1);

    const int rndLife = 100 + TickV - DATA::gc_minLife + 1;
    p->life = DATA::gc_minLife + rand() % rndLife;

    p->power = rand() % 256;
    // 눈색은 너무 어둡지 않게
    p->power = max(p->power, 64);


    p->vectorX = GetRandom_Vector_H();
    p->vectorY = GetRandom_Vector_V();
    if(p->power >= DATA::gc_minSizeBigSnow)
        p->vectorX /= 2; // 큰유닛은 흔들림이 50%
    p->maxWave = -p->vectorX;

    p->Immune_Wind = 0;
    p->cntCollison = 0;
}
void DrawPixel(int x, int y, TPixelColor cor)
{
    if(x < 0) return;
    if(x >= DATA::gc_ScreenSizeWidth) return;
    if(y < 0) return;
    if(y >= DATA::gc_ScreenSizeHeight) return;
    // 비효율의 극치지만 나는 매우 귀찮다...
    TPixelColor ori = GetPixel(DATA::g_pBitmapB, x, y);
    if(cor <= ori)
        return;

    SetPixel(DATA::g_pBitmapB, x, y, cor);
    return;
}
void DrawSnow(int x, int y, TSnowUnit& unit)
{
    UINT32 power = unit.power;
    
    // 수명이 다해가는 것은 어둡게
    //if(unit.life < 32)
    {
        power = power * unit.life / 32;
        power = min(power, unit.power);

    }

    byte r, g, b;
    BOOL bDebugSnowUnit;
#ifdef _DEBUG
    bDebugSnowUnit= TRUE;
#else
    bDebugSnowUnit = (DATA::g_flagViewMode==1)? TRUE :FALSE;
#endif
    if(bDebugSnowUnit)
    {
        if(unit.Immune_Wind)
            r=255, g=0, b=0;
        else if(unit.cntCollison)
            r=0, g=255, b=0;
        else
            r=g=b=power;
    }
    else
    {
        r=g=b=power;
    }

    TPixelColor c0(r, g, b);
    if(DATA::gc_minSizeBigSnow <= power)
    {
        // 좀더 크게 그릴까....
        DrawPixel(x, y, c0);

        TPixelColor c1(r/2, g/2, b/2);
        TPixelColor c2(r/3, g/3, b/3);

        DrawPixel(x-1, y, c1);
        DrawPixel(x+1, y, c1);
        DrawPixel(x, y-1, c1);
        DrawPixel(x, y+1, c1);
        
        DrawPixel(x-1, y-1, c2);
        DrawPixel(x+1, y-1, c2);
        DrawPixel(x-1, y+1, c2);
        DrawPixel(x+1, y+1, c2);
    }
    else
    {
        DrawPixel(x, y, c0);
    }
}

void SetPixel_to_Ground(int x, int y, UINT32 c)
{
    if(x<0) return;
    if(x>=DATA::gc_ScreenSizeWidth) return;
    if(y<0) return;
    if(y>=DATA::gc_ScreenSizeHeight) return;
    //TPixelColor rgb = ::GetPixel(DATA::g_pBitmapBG, x, y);
    //UINT32 r = rgb.R + c;
    //UINT32 g = rgb.G + c;
    //UINT32 b = rgb.B + c;
    //::SetPixel(DATA::g_pBitmapBG, x, y, TPixelColor(min(255,r), min(255,g), min(255,b)));

    assert(DATA::g_Map[x+DATA::gc_maxHorizonWave][y].GetSnowColor() == c);
    ::SetPixel(DATA::g_pBitmapBG, x, y, TPixelColor(min(255,c), min(255,c), min(255,c)));
}




BOOL Query_isValid_xM_yM(int xM, int yM)
{
    if(xM < 0) return FALSE;
    if(xM >= DATA::gc_MapSizeWidth) return FALSE;
    if(yM < 0) return FALSE;
    if(yM >= DATA::gc_MapSizeHeight) return FALSE;
    return TRUE;
}
BOOL Query_InScreen(int xM, int yM)
{
    int x = xM - DATA::gc_maxHorizonWave;
    int y = yM;
    if(x<0) return FALSE;
    if(x>=DATA::gc_ScreenSizeWidth) return FALSE;
    if(y<0) return FALSE;
    if(y>=DATA::gc_ScreenSizeHeight) return FALSE;
    return TRUE;
}

void TestNearPixel_PossibleSnowAttach(int xM, int yM)
{
    if(!Query_isValid_xM_yM(xM, yM))
        return;

    if(0 == DATA::g_Map[xM][yM].at)
        DATA::g_Map[xM][yM].at = -1;
}
void CloseThisPixel(int xM, int yM)
{
    DATA::g_Map[xM][yM].at = DATA::gc_AT_Snow;

    TestNearPixel_PossibleSnowAttach(xM, yM-1);
    TestNearPixel_PossibleSnowAttach(xM-1, yM);
    TestNearPixel_PossibleSnowAttach(xM+1, yM);
    TestNearPixel_PossibleSnowAttach(xM, yM+1);
}
void CollisionProcess_Near(int xM, int yM, UINT32 c);
BOOL Snow__dynamic_to_static(int xM, int yM, TSnowUnit& unit)
{
    if(!Query_InScreen(xM, yM))
        return TRUE; // 제거

    using namespace DATA;
    if(unit.power < 4) return TRUE;

    assert(-1 == g_Map[xM][yM].at);

    // 일정 확률로 미끄러진다
    if(unit.life >= 32)
    {
        return FALSE;
        if(64<=rand() % unit.power)
            return FALSE;
    }
    

     //일정 확률로 유닛은 그냥 사라진다
    //if(0 != rand()%5)
    //    return TRUE; // 제거
    

    {
        //UINT32 c0 = (UINT32)(unit.power*0.6);
        UINT32 c0 = (UINT32)(unit.power*1);
        DATA::g_Map[xM][yM].IncrementSnowColor(c0);
        SetPixel_to_Ground(xM-gc_maxHorizonWave, yM, g_Map[xM][yM].GetSnowColor());
    }

    //// 주변 번짐(이것을 처리하지 않으면 매우 흉하다)
    //{
    //    UINT32 c1 = (UINT32)(unit.power*0.2);
    //    //CollisionProcess_Near(xM, yM-1, c1);
    //    CollisionProcess_Near(xM, yM+1, c1);
    //    CollisionProcess_Near(xM-1, yM, c1);
    //    CollisionProcess_Near(xM+1, yM, c1);

    //    UINT32 c2 = (UINT32)(unit.power*0.1);
    //    //CollisionProcess_Near(xM-1, yM-1, c2);
    //    CollisionProcess_Near(xM-1, yM+1, c2);
    //    //CollisionProcess_Near(xM+1, yM-1, c2);
    //    CollisionProcess_Near(xM+1, yM+1, c2);
    //}

    // 눈이 충분히 쌓이면 주변 타일을 갱신
    BOOL bClose = FALSE;;
    if(g_Map[xM][yM].GetSnowColor() >= 255)
        bClose = TRUE;
    else if(g_Map[xM][yM].GetSnowColor() > 200)
    {
        const int _SelCode = 0;
        if(_SelCode==0)
        {
            bClose = (0==rand()%10);
        }
        else if(_SelCode==1)
        {
            //// 주변 타일의 색과 비슷하게 맞추기 위하여...
            //BOOL bRealWhite = TRUE;
            //if(g_Map[xM-1][yM].at == DATA::gc_AT_Snow && g_Map[xM-1][yM].GetSnowColor() != 255) bRealWhite = FALSE;
            //else if(g_Map[xM+1][yM].at == DATA::gc_AT_Snow && g_Map[xM+1][yM].GetSnowColor() != 255) bRealWhite = FALSE;
            //else if(g_Map[xM][yM+1].at == DATA::gc_AT_Snow && g_Map[xM][yM+1].GetSnowColor() != 255) bRealWhite = FALSE;

            //if(bRealWhite)
            //    bClose = (0==rand()%10);
            //else
            //    bClose = (0==rand()%5);
        }
    }

    if(bClose)
        CloseThisPixel(xM, yM);

    g_stats_SnowUnitStack_Counting++;
    return TRUE;
}
void CollisionProcess_Near(int xM, int yM, UINT32 c)
{
    if(xM < 0) return;
    if(xM >= DATA::gc_MapSizeWidth) return;
    if(yM < 0) return;
    if(yM >= DATA::gc_ScreenSizeHeight) return;
    
    if(DATA::g_Map[xM][yM].at == -1)
        return;
    if(DATA::g_Map[xM][yM].GetSnowColor() > 128)
        return;

    DATA::g_Map[xM][yM].IncrementSnowColor(c);
    const int x = xM - DATA::gc_maxHorizonWave;
    const int y = yM;
    SetPixel_to_Ground(x, y, DATA::g_Map[xM][yM].GetSnowColor());
}
void UpdateStats()
{
    if(DATA::g_stats_FPS <= 0.f)
        return;
    if(DATA::g_stats_CountingFrame_per_This_Second <= 0)
        DATA::g_stats_CountingFrame_per_This_Second = 1;

    double fps = (double)DATA::g_stats_FPS;
    DATA::g_stats_SnowUnitActive_Average = DATA::g_stats_SnowUnitActive_Counting / DATA::g_stats_CountingFrame_per_This_Second;
    DATA::g_stats_SnowUnitDraw_Average = DATA::g_stats_SnowUnitDraw_Counting / DATA::g_stats_CountingFrame_per_This_Second;
    DATA::g_stats_SnowUnitCollision_Average = DATA::g_stats_SnowUnitCollision_Counting / DATA::g_stats_CountingFrame_per_This_Second;
    DATA::g_stats_SnowUnitStack_Average = DATA::g_stats_SnowUnitStack_Counting / DATA::g_stats_CountingFrame_per_This_Second;
    DATA::g_stats_SnowUnitCollapse_Average = DATA::g_stats_SnowUnitCollapse_Counting / DATA::g_stats_CountingFrame_per_This_Second;

    DATA::g_stats_SnowUnitActive_Counting = 0;
    DATA::g_stats_SnowUnitDraw_Counting = 0;
    DATA::g_stats_SnowUnitCollision_Counting = 0;
    DATA::g_stats_SnowUnitStack_Counting = 0;
    DATA::g_stats_SnowUnitCollapse_Counting = 0;

    DATA::g_stats_CountingFrame_per_This_Second = 0;
}
void Draw_InfoText()
{
    if(!DATA::g_flagShowInfoText)
        return;

    auto oldBkMode = SetBkMode(DATA::g_hdcB, TRANSPARENT);
    {
        DWORD r, g, b;
        const char* pSTR;
        if(DATA::g_flagPause)
        {
            pSTR = "Pause", r=255, g=0, b=0;
        }
        else
        {
            switch(DATA::g_flagMode)
            {
            case DATA::E_MODE::E_Benchmark:
                pSTR = "Benchmarking", r=255, g=255, b=0;
                break;

            case DATA::E_MODE::E_Game_Poop:
                pSTR = "PoopGame", r=255, g=255, b=0;
                break;

            default:
                if(DATA::g_flagAutoEnvironment)
                    pSTR = "Automatic", r=0, g=255, b=255;
                else
                    pSTR = "Manual", r=255, g=255, b=255;
            }
        }

        int len = (int)strlen(pSTR);
        RECT rectTemp = {0};
        int hightTextImage = DrawTextA(DATA::g_hdcB, pSTR, len, &rectTemp, DT_CALCRECT);
        RECT rect ={
            (DATA::gc_ScreenSizeWidth - rectTemp.right) / 2,
            3,
            DATA::gc_ScreenSizeWidth,
            DATA::gc_ScreenSizeHeight
        };
        RECT rectShadow ={
            1 + rect.left,
            1 + rect.top,
            rect.right,
            rect.bottom
        };

        SetTextColor(DATA::g_hdcB, RGB(0, 0, 0));
        DrawTextA(DATA::g_hdcB, pSTR, len, &rectShadow, DT_LEFT | DT_TOP);

        SetTextColor(DATA::g_hdcB, RGB(r,g,b));
        DrawTextA(DATA::g_hdcB, pSTR, len, &rect, DT_LEFT | DT_TOP);
        

    }
    {
        RECT rect ={
            DATA::gc_ScreenSizeWidth - 200,
            3,
            DATA::gc_ScreenSizeWidth-6,
            DATA::gc_ScreenSizeHeight
        };
        RECT rectShadow ={
            rect.left,
            1 + rect.top,
            rect.right + 1,
            rect.bottom
        };

        const char* str_Vision[] =
        {
            "Normal",
            "Dynamic Snow",
            "Static Snow",
            "Collision Map",
            "Unknown"
        };
        int selV = DATA::g_flagViewMode;
        if(selV >= sizeof(str_Vision) / sizeof(*str_Vision))
            selV = sizeof(str_Vision) / sizeof(*str_Vision) - 1;


        char str[1024];
        int len = sprintf_s(str,
            "Help  F1 Key\n"
            "Benchmarking  F8 Key\n"
            "PoopGame F9 Key\n"
            "Hide F12 Key\n"
            "------- State -------\n"
            "%s(View)\n"
            "%+4d Wind Speed\n"
            "%8.3f FPS\n"
            , str_Vision[selV]
            , DATA::g_Wind
            , DATA::g_stats_FPS);
        switch(DATA::g_flagMode)
        {
        case DATA::E_MODE::E_Benchmark:
        case DATA::E_MODE::E_Game_Poop:
            len += sprintf_s(str+len, sizeof(str)-len, "(%d)Limit FPS", (int)(DATA::gc_fps * DATA::g_Speed));
            break;

        default:
            len += sprintf_s(str+len, sizeof(str)-len, "Unlimited FPS");
        }

        SetTextColor(DATA::g_hdcB, RGB(0,128,255));
        DrawTextA(DATA::g_hdcB, str, len, &rectShadow, DT_RIGHT | DT_TOP);

        SetTextColor(DATA::g_hdcB, RGB(255,128,0));
        DrawTextA(DATA::g_hdcB, str, len, &rect, DT_RIGHT | DT_TOP);
    }
    {
        RECT rect ={
            6,
            3,
            270,
            DATA::gc_ScreenSizeHeight
        };
        RECT rectShadow ={
            1 + rect.left,
            1 + rect.top,
            rect.right,
            rect.bottom
        };
        char str1[1024];
        int len1 = sprintf_s(str1,
            "Limit Snow Instance  %10d\n"
            "------------------ Stats (per Frame) ------------------\n"
            "Snow Active\n"
            "Snow Draw\n"
            "Snow Collision\n"
            "Snow Stack\n"
            "Snow Collapse\n"
            , DATA::g_nUsingSnowUnits
            );
        char str2[1024];
        int len2 = sprintf_s(str2,
            "\n"
            "\n"
            "%llu (100.00%%)\n"
            "%llu (%06.2f%%)\n"
            "%llu (%06.2f%%)\n"
            "%llu (%06.2f%%)\n"
            "%llu (%06.2f%%)\n"
            , DATA::g_stats_SnowUnitActive_Average
            
            , DATA::g_stats_SnowUnitDraw_Average
            , (DATA::g_stats_SnowUnitActive_Average>0.)? 100. * DATA::g_stats_SnowUnitDraw_Average / DATA::g_stats_SnowUnitActive_Average : 0.
            
            , DATA::g_stats_SnowUnitCollision_Average
            , (DATA::g_stats_SnowUnitActive_Average>0.)? 100. * DATA::g_stats_SnowUnitCollision_Average / DATA::g_stats_SnowUnitActive_Average : 0.

            , DATA::g_stats_SnowUnitStack_Average
            , (DATA::g_stats_SnowUnitActive_Average>0.)? 100. * DATA::g_stats_SnowUnitStack_Average / DATA::g_stats_SnowUnitActive_Average : 0.

            , DATA::g_stats_SnowUnitCollapse_Average
            , (DATA::g_stats_SnowUnitActive_Average>0.)? 100. * DATA::g_stats_SnowUnitCollapse_Average / DATA::g_stats_SnowUnitActive_Average : 0.
            );
        SetTextColor(DATA::g_hdcB, RGB(0,0,0));
        DrawTextA(DATA::g_hdcB, str1, len1, &rectShadow, DT_LEFT | DT_TOP);
        SetTextColor(DATA::g_hdcB, RGB(0,255,0));
        DrawTextA(DATA::g_hdcB, str1, len1, &rect, DT_LEFT | DT_TOP);

        rectShadow.right += 1;
        SetTextColor(DATA::g_hdcB, RGB(0,0,0));
        DrawTextA(DATA::g_hdcB, str2, len2, &rectShadow, DT_RIGHT | DT_TOP);
        SetTextColor(DATA::g_hdcB, RGB(64,255,128));
        DrawTextA(DATA::g_hdcB, str2, len2, &rect, DT_RIGHT | DT_TOP);
        
    }

    const size_t SizeBuffer = 1024;
    char str[SizeBuffer];
    size_t len = 0;
    switch(DATA::g_flagMode)
    {
    case DATA::E_MODE::E_Benchmark:
        len = GFN::Second_to_HHMMSS(str, SizeBuffer, DATA::g_Benchmark_TimeRemaining / DATA::g_Benchmark_Tick_per_Sec);
        break;

    case DATA::E_MODE::E_Game_Poop:
        {
            float fLife = 0.f;
            if((float)DATA::g_PoopGamePlayer.Setting.sc_Size_MaxRadius > DATA::g_PoopGamePlayer.fSize)
            {
                float fLifeMax = DATA::g_PoopGamePlayer.Setting.sc_Size_MaxRadius - DATA::g_PoopGamePlayer.Setting.sc_Size_MinRadius;
                fLife = (float)DATA::g_PoopGamePlayer.Setting.sc_Size_MaxRadius - DATA::g_PoopGamePlayer.fSize;
                fLife = fLife / fLifeMax * 100.f;
            }

            len = sprintf_s(str, SizeBuffer, "Score %llu\n"
                "Life %u%%"
                , (UINT64)DATA::g_PoopGame_Score
                , (UINT32)fLife);
        }
        break;
    default:
        if(0 < DATA::g_Benchmark_Score)
        {
            len += sprintf_s(str+len,
                SizeBuffer - len,
                "--- Benchmark Result ---\n"
                "Best Score\n"
                "%llu\n"
                "Last Score\n"
                "%llu\n"
                , DATA::g_Benchmark_Score_Best
                , DATA::g_Benchmark_Score);
        }
        if(0 < DATA::g_PoopGame_Score)
        {
            len += sprintf_s(str+len,
                SizeBuffer - len,
                "--- PoopGame Result ---\n"
                "Best Score\n"
                "%llu\n"
                "Last Score\n"
                "%llu\n"
                , DATA::g_PoopGame_Score_Best
                , (UINT64)DATA::g_PoopGame_Score);
        }
    }
    if(0 < len)
    {
        RECT rectTemp = {0};
        int hightTextImage = DrawTextA(DATA::g_hdcB, str, len, &rectTemp, DT_CALCRECT);
        RECT rect ={
            (DATA::gc_ScreenSizeWidth - rectTemp.right) / 2,
            DATA::gc_ScreenSizeHeight - hightTextImage-6,
            DATA::gc_ScreenSizeWidth,
            DATA::gc_ScreenSizeHeight-6
        };
        RECT rectShadow ={
            1 + rect.left,
            1 + rect.top,
            rect.right,
            rect.bottom
        };

        SetTextColor(DATA::g_hdcB, RGB(0,0,0));
        DrawTextA(DATA::g_hdcB, str, len, &rectShadow, DT_LEFT | DT_TOP);

        SetTextColor(DATA::g_hdcB, RGB(255,255,0));
        DrawTextA(DATA::g_hdcB, str, len, &rect, DT_LEFT | DT_TOP);
    }
    SetBkMode(DATA::g_hdcB, oldBkMode);
}
void DrawATMap_or_SnowMap()
{
    // g_flagViewMode
    //  2 달라 붙은 눈
    //  3 AT MAP
    for(int x=0; x<DATA::gc_ScreenSizeWidth; x++)
    {
        for(int y=0; y<DATA::gc_ScreenSizeHeight; y++)
        {
            auto& ref = DATA::g_Map[x+DATA::gc_maxHorizonWave][y];
            auto p = DATA::g_pBitmapB + x*DATA::gc_ScreenSizeHeight + y;
            
            byte r, g, b;
            if(DATA::g_flagViewMode == 2)
            {
                r=g=b= ref.GetSnowColor();
            }
            else if(DATA::g_flagViewMode == 3)
            {
                switch(ref.at)
                {
                case 0:
                    r=g=b=0;
                    if(ref.snow_unit)
                    {
                        UINT32 i32B = (UINT32)16*ref.snow_unit;
                        if(i32B < 128)
                            i32B = 128;
                        else if(255 < i32B)
                            i32B = 255;
                        g=b = i32B;
                    }
                    break;
                case -1:
                    r=255, g=0, b=255;
                    break;
                case DATA::gc_AT_Snow:
                    r=g=b=255;
                    break;
                case DATA::gc_AT_Object:
                    r=0, g=255, b=0;
                    break;
                case DATA::gc_AT_Ground:
                    r=0, g=0, b=255;
                    break;
                default:
                    assert(0);
                    r=0, g=255, b=255;
                }
            }
            else
            {
                r=g=b= 0;
            }
            SetPixel(DATA::g_pBitmapB, x, y, TPixelColor(r, g, b));
        }
    }
}
BOOL FindOpenDirection_and_ReVector(TSnowUnit& u, float fVecX, float fVecY, float fDistance)
{
    

    // fVecX fVecY : Normal
    using namespace DATA;

    int xM = (int)(u.x+0.5f) + gc_maxHorizonWave;
    int yM = (int)(u.y+0.5f);

    struct TVector2{
        float x, y;
    };
    const float cN = 0.7071067811865475f;
    // 0 1 2
    // 3   4
    // 5 6 7
    const POINT arrPT[]=
    {
        {-1, -1}, {0, -1}, {+1, -1},
        {-1, +0},          {+1, +0},
        {-1, +1}, {0, +1}, {+1, +1},
    };
    const TVector2 arrNOR[] =
    {
        {-cN, -cN}, {0.f, -1.f}, {+cN, -cN},
        {-1, 0.f},               {+1.f, 0.f},
        {-cN, +cN}, {0.f, +1.f}, {+cN, +cN},
    };


    // 진행방향
    const int SequenceBL[] = {5, 6, 7, 3, 4, 0, 1, 2};
    const int SequenceBR[] = {7, 6, 5, 4, 3, 2, 1, 0};
    const int SequenceLB[] = {5, 3, 0, 6, 1, 7, 4, 2};
    const int SequenceRB[] = {7, 4, 2, 6, 1, 5, 3, 0};

    const int SequenceTL[] = {0, 1, 2, 3, 4, 5, 6, 7};
    const int SequenceTR[] = {2, 1, 0, 4, 3, 7, 6, 5};
    const int SequenceLT[] = {0, 3, 5, 1, 6, 2, 4, 7};
    const int SequenceRT[] = {2, 4, 7, 1, 6, 0, 3, 5};
    
    
    bool isLeftDirection;
    if(fVecX < 0.f)
        isLeftDirection = true;
    else if(fVecX > 0.f)
        isLeftDirection = false;
    else
        isLeftDirection = (rand()%2 == 0)? true : false;
    
    bool isUpDirection;
    if(fVecY < 0.f)
        isUpDirection = true;
    else if(fVecY > 0.f)
        isUpDirection = false;
    else
        isUpDirection = (rand()%2 == 0)? true : false;


    const int* pSelect_Sequence;
    if(abs(fVecY) >= abs(fVecX))
    {
        if(isUpDirection)
            pSelect_Sequence = (isLeftDirection)? SequenceTL : SequenceTR;
        else
            pSelect_Sequence = (isLeftDirection)? SequenceBL : SequenceBR;
    }
    else
    {
        if(isUpDirection)
            pSelect_Sequence = (isLeftDirection)? SequenceLT : SequenceRT;
        else
            pSelect_Sequence = (isLeftDirection)? SequenceLB : SequenceRB;
    }

    // 추가기능
    // 이전 스카라를값를 감쇄하여 사용 최소값은 1
    // 이전 벡터 x ,y 의 비율을 서로 교환하여
    // 튕겨내도록
    for(int i=0; i<8; i++)
    {
        const POINT& pt = arrPT[pSelect_Sequence[i]];
        const TVector2& nor = arrNOR[pSelect_Sequence[i]];

        if(!Query_isValid_xM_yM(xM+pt.x, yM+pt.y))
            continue;

        if(g_Map[xM+pt.x][yM+pt.y].Query_Is_Full())
            continue;
        if(0 < g_Map[xM+pt.x][yM+pt.y].at)
            continue;

        //----------------------------------------------------------------
        //  #1
    
        //u.vectorY = (char)pt.y;
        //if(0 < pt.y)
        //    u.vectorY *= (gc_minVerticalSpeed*10);

        //u.vectorX = (char)pt.x * (rand() % (1 + gc_maxHorizonWave*10));
        //----------------------------------------------------------------
        //  #2

        //float fNewScalar = fDistance * 0.75f; // 감쇄할것
        // 이전 수직 수평벡터를 교환
        //float fNewVecX = 10 * fNewScalar * abs(fVecY);
        //float fNewVecY = 10 * fNewScalar * abs(fVecX);
        //if(fNewVecX > 100)
        //    fNewVecX = 100;
        //if(fNewVecY > 100)
        //    fNewVecY = 100;

        //// 새로운 방향 적용
        //fNewVecX *= pt.x;
        //fNewVecY *= pt.y;
        //u.vectorX = (char)fNewVecX;
        //u.vectorY = (char)fNewVecY;
        //----------------------------------------------------------------
        //  #3
        //float fNewScalar = fDistance * 0.8f; // 감쇄할것
        //u.vectorX = 10 * fNewScalar * -fVecX;
        //u.vectorY = 10 * fNewScalar * fVecY;
        //----------------------------------------------------------------
        //  #4
        float fNewScalar = fDistance * 0.5f; // 감쇄할것
        if(fNewScalar < 1.f)
            fNewScalar = 1.f;
        else if(fNewScalar > DATA::g_fmaxSnowVectorDistance)
            fNewScalar = DATA::g_fmaxSnowVectorDistance;
        u.vectorX = (char)(10 * fNewScalar * nor.x);
        u.vectorY = (char)(10 * fNewScalar * nor.y);
        //if(abs(u.vectorX) > DATA::gc_maxHorizonWave)
        //    u.vectorX = u.vectorX / abs(u.vectorX) * DATA::gc_maxHorizonWave;
        //if(abs(u.vectorY) > DATA::gc_maxVerticalSpeed)
        //    u.vectorY = u.vectorY / abs(u.vectorY) * DATA::gc_maxVerticalSpeed;

        assert(u.vectorX * pt.x >= 0);
        assert(u.vectorY * pt.y >= 0);



        //----------------------------------------------------------------
        if(0 != g_Wind/* || 0 < pt.x * g_Wind*/)
        {
            u.Immune_Wind = 4;
        }
        u.x += (pt.x);

        if(u.cntCollison != 255)
            u.cntCollison++;

        return TRUE;
    }

    return FALSE;
}

// 교점을 리턴하도록 한다
BOOL FindIntersection_and_RePos(TSnowUnit& u, POINT& pt)
{
    // 유닛은 화면에 들어온것만을 테스트
#ifdef _DEBUG
    static UINT64 stats0 = 0;
    static UINT64 stats1 = 0;
    static UINT64 stats2 = 0;
    static UINT64 stats3 = 0;
    #define INCREMENT_CNT(n) stats##n ++;
#else
    #define INCREMENT_CNT(n) __noop
#endif

    float backupX = u.x;
    float backupY = u.y;
    assert(Query_isValid_xM_yM((int)(u.x+0.5f)+DATA::gc_maxHorizonWave, (int)(u.y+0.5f)));

    using namespace DATA;
    float fVecX = u.vectorX/10.f;
    if(!u.Immune_Wind)
        fVecX += g_Wind;
    float fVecY = u.vectorY/10.f;
    float xS = u.x - fVecX;
    float yS = u.y - fVecY;
    float fDistance = sqrt((fVecX*fVecX + fVecY*fVecY));
    fVecX /= fDistance;
    fVecY /= fDistance;
    assert(fVecX!=0.f || fVecY!=0.f);

    // 우선적으로 현재지점에서 탈출 방향을 찾는다면,
    // 속도는 빠르나 적확도가 떨어진다
    //if(FindOpenDirection_and_ReVector(u, fVecX, fVecY, fDistance))
    //{
    //    INCREMENT_CNT(0);
    //    return FALSE;
    //}

    for(float fD=0.f; fD<fDistance; fD += 1.f)
    {
        float fx = xS + fVecX * fD;
        float fy = yS + fVecY * fD;
        int x = (int)(fx+0.5f);
        int y = (int)(fy+0.5f);
        int xM = x + gc_maxHorizonWave;
        int yM = y;
        
        if(!Query_isValid_xM_yM(xM, yM))
            continue;

        if(0 < g_Map[xM][yM].at || g_Map[xM][yM].Query_Is_Full())
        {
            u.x = fx -fVecX;//*0.5f;
            u.y = fy -fVecY;//*0.5f;

            if(FindOpenDirection_and_ReVector(u, fVecX, fVecY, fDistance))
            {
                INCREMENT_CNT(1);
                pt.x = xM, pt.y = yM;

                g_stats_SnowUnitCollision_Counting++;
                return TRUE;
            }
            else if(g_Map[(int)(u.x+0.5f)+gc_maxHorizonWave][(int)(yM+0.5f)].at == -1)
            {
                INCREMENT_CNT(2);
                return FALSE;
            }
        }
    }

    u.x = backupX, u.y = backupY;
    if(FindOpenDirection_and_ReVector(u, fVecX, fVecY, fDistance))
        return FALSE;

    // 이경우 눈을 쌓이게 할 것인가? 또는 제거?
    u.life = 0;
    INCREMENT_CNT(3);
    return FALSE;

#undef INCREMENT_CNT
}
void Debug_Find_NearFreeSlot(TSnowUnit& u)
{
    int x = (int)(u.x + 0.5f) + DATA::gc_maxHorizonWave;
    int y = (int)(u.y + 0.5f);

    int cnt = 0;
    for(int i=x-1; i<=x+1; i++)
    {
        for(int j=y-1; j<=y+1; j++)
        {
            if(!Query_isValid_xM_yM(i, j))
                continue;
            if(DATA::g_Map[i][j].at == -1)
                cnt++;
        }
    }
    if(!cnt)
    {
        OutputDebugStringA("Not Found\n");
    }
    else
    {
        char str[256];
        sprintf_s(str, "Found : %d\n", cnt);
        OutputDebugStringA(str);
    }
}

void PixelAT_Test_isValid(int xM, int yM)
{
    if(!Query_isValid_xM_yM(xM, yM))
        return;

    if(DATA::g_Map[xM][yM].at != -1)
        return;

    // 주변타일 검사
    for(int i=xM-1; i<=xM+1; i++)
    {
        for(int j=yM-1; j<=yM+1; j++)
        {
            if(!Query_isValid_xM_yM(i, j))
                continue;
            switch(DATA::g_Map[i][j].at)
            {
            case DATA::gc_AT_Snow:
            case DATA::gc_AT_Ground:
            case DATA::gc_AT_Object:
                return;
            }
        }
    }

    DATA::g_Map[xM][yM].at = 0;
}
void PixelAT_Test_isValid_NearPixel(int xM, int yM)
{
    //// 만약 윗칸에 눈이 존재한다면 침식시킨다
    //if(Query_isValid_xM_yM(xM, yM-1))
    //{
    //    if(DATA::g_Map[xM][yM-1].at == DATA::gc_AT_Snow)
    //    {
    //        DATA::g_Map[xM][yM].at = DATA::gc_AT_Snow;
    //        DATA::g_Map[xM][yM].SetSnowColor( DATA::g_Map[xM][yM-1].GetSnowColor() );
    //        SetPixel_to_Ground(xM - DATA::gc_maxHorizonWave, yM, DATA::g_Map[xM][yM].GetSnowColor());

    //        DATA::g_Map[xM][yM-1].at = -1;
    //        DATA::g_Map[xM][yM-1].SetSnowColor(0);
    //        SetPixel_to_Ground(xM - DATA::gc_maxHorizonWave, yM-1, DATA::g_Map[xM][yM-1].GetSnowColor());

    //        PixelAT_Test_isValid_NearPixel(xM, yM-1);
    //        return;
    //    }
    //}

    for(int i=xM-1; i<=xM+1; i++){
        for(int j=yM-1; j<=yM+1; j++){
            PixelAT_Test_isValid(i, j);
        }
    }
}
void TestCollapseGroundSnow_from_DynamicSnow(TSnowUnit& u, POINT& pt)
{
    assert(DATA::g_Map[pt.x][pt.y].at == DATA::gc_AT_Snow);
    int cntSomething[3] = {0}; // T M B
    for(int i=pt.x-1; i<=pt.x+1; i++)
    {
        for(int j=pt.y-1; j<=pt.y+1; j++)
        {
            if(j==0) continue; 

            int indexCNT = pt.y - j + 1;
            if(!Query_isValid_xM_yM(i, j))
                cntSomething[indexCNT]++;
            else if(DATA::g_Map[i][j].at <= 0)
                cntSomething[indexCNT]++;
        }
    }
    if(cntSomething[2] == 3)
        return;

    int cntSomething_Total = cntSomething[0] + cntSomething[1] + cntSomething[2];
    if(cntSomething_Total >= 6)
        return;

    //if(0 < rand() % (cntSomething_Total*10+1))
    //    return;

    UINT32 sumLight = 0;
    sumLight = DATA::g_Map[pt.x][pt.y].GetSnowColor();
    DATA::g_Map[pt.x][pt.y].SetSnowColor(0);
    DATA::g_Map[pt.x][pt.y].at = -1;
    SetPixel_to_Ground(pt.x - DATA::gc_maxHorizonWave, pt.y, 0);
    PixelAT_Test_isValid_NearPixel(pt.x, pt.y);

    // 자연스럽기 위해서 주변타일을 흡수한다
    const BOOL bEraseNearSnow = 0;
    if(bEraseNearSnow)
    {
        for(int i=pt.x-1; i<=pt.x+1; i++)
        {
            for(int j=pt.y-1; j<=pt.y+1; j++)
            {
                if(!Query_isValid_xM_yM(i, j))
                    continue;
                if(DATA::g_Map[i][j].at != DATA::gc_AT_Snow)
                    continue;

                sumLight += DATA::g_Map[i][j].GetSnowColor();
                DATA::g_Map[i][j].SetSnowColor(0);
                DATA::g_Map[i][j].at = -1;
                SetPixel_to_Ground(i - DATA::gc_maxHorizonWave, j, 0);
                PixelAT_Test_isValid_NearPixel(i, j);
            }
        }
    }
    sumLight += u.power;
    u.power = max(255, sumLight);
    //u.life  = 255;

    //int xUM = (int)(u.x+0.5f)+DATA::gc_maxHorizonWave;
    //int yUM = (int)(u.y+0.5f);
    //if(DATA::g_Map[xUM][yUM].at == -1)
    //{
    //    // 유닛이 위치한 점이 눈이 확장 가능한 영역이라면 속성 제거
    //    DATA::g_Map[xUM][yUM].at = 0;
    //    DATA::g_Map[xUM][yUM].SetSnowColor(0);
    //    SetPixel_to_Ground(xUM - DATA::gc_maxHorizonWave, yUM, 0);
    //}

    DATA::g_stats_SnowUnitCollapse_Counting++;
}
#include<typeinfo>
void AutoWind();
void AutoSnowIncrement();
void FrameProcess()
{
    using namespace DATA;
    const int W = gc_ScreenSizeWidth;
    const int H = gc_ScreenSizeHeight;
    int cntRecycle = 0;
    const int limitRecycle = (g_nUsingSnowUnits / gc_ScreenSizeHeight) * (gc_minVerticalSpeed+gc_maxVerticalSpeed);
    

    int nEND = max(g_iLastLiveUnitIndex, g_nUsingSnowUnits-1);
    for(int i=0; i<=nEND; i++)
    {
        TSnowUnit& unit = g_SnowUnits[i];
        {
            int xF = (int)(unit.x+0.5f);
            int yF = (int)(unit.y+0.5f);
            if(yF < 0){}
            else if(yF >= H){}
            else if(xF < 0){}
            else if(xF >= W){}
            else
            {
                xF+=gc_maxHorizonWave;
                if(!g_Map[xF][yF].DetachSnowUnit())
                    unit.life = 0;
            }
        }

        if(0 == unit.life)
        {
            if(i<g_nUsingSnowUnits && cntRecycle < limitRecycle)
            {
                Recycle(&unit);
                cntRecycle++;
                continue;
            }
            else
            {
                continue;
            }
        }
        else
        {
            g_stats_SnowUnitActive_Counting++;
        }

        g_iLastLiveUnitIndex = i;
        unit.life--;

        {
            // 위로 올라가거나 너무 느린 유닛 낙하 가속
            if(unit.vectorY < gc_minVerticalSpeed*10)
                unit.vectorY+=2;

            // 수평속도 제한
            if(abs(unit.vectorX) > gc_maxHorizonWave * 10)
            {
                assert(typeid(unit.vectorX) == typeid(char));
                char t = unit.vectorX / abs(unit.vectorX) * 1;
                unit.vectorX -= t;
            }
            else if(DATA::g_Wind ==0 && !unit.Immune_Wind /*&& unit.power < DATA::gc_minSizeBigSnow*/)
            {
                const char _array_HorizonWaveStep[] = {0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1};
                const int _maxHorizonWaveStep = sizeof(_array_HorizonWaveStep) / sizeof(*_array_HorizonWaveStep);

                int iVal = _array_HorizonWaveStep[g_iSel_HorizonWaveStep++];
                if(g_iSel_HorizonWaveStep == _maxHorizonWaveStep)
                    g_iSel_HorizonWaveStep = 0;
                // 측면바람 저항중이 아니라면
                // 좌우로 흔들린다
                if(unit.maxWave < 0)
                {
                    if(unit.vectorX <= unit.maxWave)
                        unit.vectorX += iVal, unit.maxWave = -unit.maxWave;
                    else
                        unit.vectorX -= iVal;
                }
                else if(unit.maxWave > 0)
                {
                    if(unit.vectorX >= unit.maxWave)
                        unit.vectorX -= iVal, unit.maxWave = -unit.maxWave;
                    else
                        unit.vectorX += iVal;
                }
            }
        }

        assert(abs(unit.vectorX) <= DATA::g_maxSnowVectorDistance*10);

        unit.x += (unit.vectorX/10.f);
        if(!unit.Immune_Wind)
            unit.x += (float)g_Wind;
        unit.y += (unit.vectorY/10.f);
        if(unit.y+0.5f >= H)
            unit.y = H-1.f;

        const int xF = (int)(unit.x+0.5f);
        const int yF = (int)(unit.y+0.5f);
        if(xF < 0)
        {
            if(g_Wind < 0)
                unit.life = 0;
            continue;
        }
        else if(W <= xF)
        {
            if(0 < g_Wind)
                unit.life = 0;
            continue;
        }
        else if(yF < 0)
        {
            continue;
        }

        const int xM = xF + gc_maxHorizonWave;
        const int yM = yF;

        // 쌓임
        BOOL bAttach;
        if(/*unit.Immune_Wind ||*/ -1 != g_Map[xM][yM].at)
        {
            bAttach = FALSE;
        }
        else
        {
            // 인접 타일 평균값과 비교
            // 가중치
            //  1   0   1
            //  2   3   2
            UINT32 sum = (UINT32)g_Map[xM-1][yM].GetSnowColor() /*+ (UINT32)g_Map[xM][yM].GetSnowColor()*/ + (UINT32)g_Map[xM+1][yM].GetSnowColor()
                + (UINT32)g_Map[xM-1][yM+1].GetSnowColor()*2 + (UINT32)g_Map[xM][yM+1].GetSnowColor()*3 + (UINT32)g_Map[xM+1][yM+1].GetSnowColor()*2;

            sum /= 9;

            if(gc_magicCODE <= sum)
                bAttach = TRUE;
            else
                bAttach = FALSE;

            if(!bAttach)
            {
                // 인접한 타일 속성을 확인
                INT16 sumAT = 0;
                for(int i=-1; i<=1; i++)
                {
                    for(int j=-1; j<=1; j++)
                    {
                        if(yM+j < 0) continue;

                        if(g_Map[xM+i][yM+j].at <= 0)
                            continue;
                        sumAT += g_Map[xM+i][yM+j].at;
                    }
                }
                if(sumAT >= 60)
                    bAttach = TRUE;
            }
        }
        if(bAttach)
        {
            if(bAttach = Snow__dynamic_to_static(xM, yM, unit))
                unit.life = 0;
        }


        // 충돌, 밀어내기
        if(!bAttach)
        {
            BOOL bCollision;
            if(0 < g_Map[xM][yM].at)
                bCollision = TRUE;
            else if(g_Map[xM][yM].Query_Is_Full())
                bCollision = TRUE;
            else
                bCollision = FALSE;

            if(bCollision)
            {
                POINT pt;
                if(FindIntersection_and_RePos(unit, pt))
                {
                    assert((int)(pt.y+0.5f) < gc_ScreenSizeHeight);
                    assert(0 <= pt.y);
                    if(g_Map[pt.x][pt.y].at == gc_AT_Snow)
                    {
                        // 동적인 눈이 정적인 쌓인 눈을 흡수
                        //      개선한다면 흡수가 아니라 새로운 눈 생성
                        // 단 이것은 시각적으로 볼때 현재 동적인 눈이 충분히 커야 어색하지 않다
  //                      if(unit.power >= gc_minSizeBigSnow)
//                            TestCollapseGroundSnow_from_DynamicSnow(unit, pt);
                    }
                }
                else if(0 < unit.life)
                {
                    int tempX = (int)(unit.x+0.5f)+gc_maxHorizonWave;
                    int tempY = (int)(unit.y+0.5f);
                    if(!Query_isValid_xM_yM(tempX, tempY))
                    {
                    }
                    else if(g_Map[tempX][tempY].at == -1)
                    {
                        if(Snow__dynamic_to_static(tempX, tempY, unit))
                            unit.life = 0;
                    }
                    else
                    {
                        //unit.life--;
                    }
                }
                if(unit.vectorY==0 && unit.vectorX==0)
                {
                    unit.vectorX = GetRandom_Vector_H();
                    unit.vectorY = GetRandom_Vector_V();
                    unit.Immune_Wind = 0;
                }
            }
            else if(unit.Immune_Wind)
            {
                unit.Immune_Wind--;
                if(0==unit.Immune_Wind)
                {
                    unit.vectorX = GetRandom_Vector_H();
                    unit.vectorY = GetRandom_Vector_V();
                }
            }
        }
        int xN = (int)(unit.x+0.5f) + gc_maxHorizonWave;
        int yN = (int)(unit.y+0.5f);
        if(0<=xN && xN < gc_MapSizeWidth && 0<=yN && yN < gc_MapSizeHeight)
            g_Map[xN][yN].AttachSnowUnit();
        if(yN >= gc_ScreenSizeHeight)
            unit.life = 0;
    }

    //if(cntRecycle == 0)
    //    UnitsIncrement();

    if(DATA::g_flagMode == DATA::E_MODE::E_Game_Poop)
        FrameProcess_PoopGame_Player();
}
void FrameProcess_PoopGame_Player()
{
    float fps = DATA::g_stats_FPS;
    if(fps <= 0.f)
        return;
    else if((float)DATA::g_PoopGamePlayer.Setting.sc_LimitFPS < fps)
        fps = (float)DATA::g_PoopGamePlayer.Setting.sc_LimitFPS;

    float spf = 0.f;
    if(0.f < fps)
        spf = 1.f / fps;

    DATA::g_PoopGame_Score += spf;



    const int c_keycodeUp = 0x41 + 'w' - 'a';
    const int c_keycodeDown = 0x41 + 's' - 'a';
    const int c_keycodeLeft = 0x41 + 'a' - 'a';
    const int c_keycodeRight = 0x41 + 'd' - 'a';

    auto s_KeyState_Up = GetAsyncKeyState(c_keycodeUp);
    auto s_KeyState_Down = GetAsyncKeyState(c_keycodeDown);
    auto s_KeyState_Left = GetAsyncKeyState(c_keycodeLeft);
    auto s_KeyState_Right = GetAsyncKeyState(c_keycodeRight);

    if(s_KeyState_Up | s_KeyState_Down | s_KeyState_Left | s_KeyState_Right)
    {
        float fSpeed = (float)DATA::g_PoopGamePlayer.Setting.sc_Speed_per_Sec * spf;
        float fMoveX = 0.f;
        float fMoveY = 0.f;
        if(s_KeyState_Up)
            fMoveY -= 1.f;
        if(s_KeyState_Down)
            fMoveY += 1.f;
        if(s_KeyState_Left)
            fMoveX -= 1.f;
        if(s_KeyState_Right)
            fMoveX += 1.f;
        float fDistance = fMoveY*fMoveY + fMoveX*fMoveX;
        fDistance = sqrt(fDistance);
        if(fMoveY != 0.f)
        {
            float fDirectionY = fMoveY / fDistance;
            float fMoveResultY = fDirectionY * fSpeed;
            DATA::g_PoopGamePlayer.fY += fMoveResultY;
        }
        if(fMoveX != 0.f)
        {
            float fDirectionX = fMoveX / fDistance;
            float fMoveResultX = fDirectionX * fSpeed;
            DATA::g_PoopGamePlayer.fX += fMoveResultX;
        }
    }

    if(DATA::g_PoopGamePlayer.fY <= 0.f)
        DATA::g_PoopGamePlayer.fY = 0.f;
    else if((float)DATA::gc_ScreenSizeHeight <= DATA::g_PoopGamePlayer.fY)
        DATA::g_PoopGamePlayer.fY = (float)DATA::gc_ScreenSizeHeight - 1.f;
    if(DATA::g_PoopGamePlayer.fX <= 0.f)
        DATA::g_PoopGamePlayer.fX = 0.f;
    else if((float)DATA::gc_ScreenSizeWidth <= DATA::g_PoopGamePlayer.fX)
        DATA::g_PoopGamePlayer.fX = (float)DATA::gc_ScreenSizeWidth - 1.f;





    // 충돌 테스트
    int sx = static_cast<int>(DATA::g_PoopGamePlayer.fX - DATA::g_PoopGamePlayer.fSize + 0.5f);
    int sy = static_cast<int>(DATA::g_PoopGamePlayer.fY - DATA::g_PoopGamePlayer.fSize + 0.5f);
    int ex = static_cast<int>(DATA::g_PoopGamePlayer.fX + DATA::g_PoopGamePlayer.fSize + 0.5f);
    int ey = static_cast<int>(DATA::g_PoopGamePlayer.fY + DATA::g_PoopGamePlayer.fSize + 0.5f);
    int cx = static_cast<int>(DATA::g_PoopGamePlayer.fX + 0.5f);
    int cy = static_cast<int>(DATA::g_PoopGamePlayer.fY + 0.5f);

    BOOL bCollision = FALSE;
    for(int y = sy; y<=ey; y++)
    {
        float fDisY = (float)(y - cy);
        float fDisYY = fDisY * fDisY;
        for(int x = sx; x<=ex; x++)
        {
            float fDisX = (float)(x - cx);
            float fDisXX = fDisX * fDisX;
            float fDisXY = sqrt(fDisYY + fDisXX);

            float fPower = 1.f - fDisXY / DATA::g_PoopGamePlayer.fSize;
            int iPower = (int)(255.f * fPower);
            if(255 < iPower)
                iPower = 255;
            else if(iPower <= 128)
                continue;

            bCollision = TRUE;

            int mX = DATA::gc_maxHorizonWave+x;
            int mY = y;
            if(mX < 0 || DATA::gc_MapSizeWidth <= mX)
                continue;
            if(mY < 0 || DATA::gc_MapSizeHeight <= mY)
                continue;

            // 허공이 아니라면 충돌 처리해야 한다
            if(!DATA::g_Map[mX][mY].snow_unit)
                continue;
            if(DATA::g_Map[mX][mY].at != 0)
                continue;

            BYTE bAdd = DATA::g_Map[mX][mY].snow_unit;

            DATA::g_PoopGamePlayer.fSize += ( (float)(bAdd) / 10.f);
            DATA::g_Map[mX][mY].snow_unit = 0;
        }
    }
    if(!bCollision)
    {
        // 크기 감소
        float fReduction = (float)DATA::g_PoopGamePlayer.Setting.sc_Recduction_per_Sec__Numerator / (float)DATA::g_PoopGamePlayer.Setting.sc_Recduction_per_Sec__Denominator;
        DATA::g_PoopGamePlayer.fSize -= (spf * fReduction);
        if(DATA::g_PoopGamePlayer.fSize < DATA::g_PoopGamePlayer.Setting.sc_Size_MinRadius)
            DATA::g_PoopGamePlayer.fSize = DATA::g_PoopGamePlayer.Setting.sc_Size_MinRadius;
    }

    if((float)DATA::g_PoopGamePlayer.Setting.sc_Size_MaxRadius <= DATA::g_PoopGamePlayer.fSize)
    {
        DATA::g_PoopGamePlayer.fSize = (float)DATA::g_PoopGamePlayer.Setting.sc_Size_MaxRadius;
        // gave over...
        DATA::g_flagMode = DATA::E_MODE::E_Simulator;
        DATA::g_flagPause = TRUE;
        if(DATA::g_PoopGame_Score_Best < (UINT64)DATA::g_PoopGame_Score)
            DATA::g_PoopGame_Score_Best = (UINT64)DATA::g_PoopGame_Score;

    }
}
extern void RenderProcess_PoopGame_Player();
void RenderProcess()
{
    using namespace DATA;
    const int W = gc_ScreenSizeWidth;
    const int H = gc_ScreenSizeHeight;

    int cntDrawSnow = 0;

    if(1 < g_flagViewMode)
    {
        DrawATMap_or_SnowMap();
    }
    else
    {
        if(g_flagViewMode == 1)
            ::FillRect(DATA::g_hdcB, &DATA::gc_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        else
            ::BitBlt(g_hdcB, 0, 0, W, H, g_hdcBG, 0, 0, SRCCOPY);

        int nEND = g_iLastLiveUnitIndex;
        for(int i=0; i<=nEND; i++)
        {
            TSnowUnit& unit = g_SnowUnits[i];
            if(0 ==  unit.life)
                continue;

            const int xF = (int)(unit.x+0.5f);
            const int yF = (int)(unit.y+0.5f);

            if(yF < 0)
                continue;
            if(xF < 0)
                continue;
            if(xF >= W)
                continue;

            cntDrawSnow++;

            DrawSnow(xF, yF, unit);
        }
    }
    if(g_flagMode == E_MODE::E_Game_Poop)
        RenderProcess_PoopGame_Player();

    Draw_InfoText();
    ::BitBlt(g_hdc, 0, 0, W, H, g_hdcB, 0, 0, SRCCOPY);
    // 바탕화면도 가능
    //auto hDeskTop = GetDC(0);
    //::BitBlt(hDeskTop, 0, 0, W, H, g_hdcB, 0, 0, SRCCOPY);
    //::ReleaseDC(0, hDeskTop);

    if(!g_flagPause)
        g_stats_SnowUnitDraw_Counting += cntDrawSnow;
}
void RenderProcess_PoopGame_Player()
{
    int sx = static_cast<int>(DATA::g_PoopGamePlayer.fX - DATA::g_PoopGamePlayer.fSize + 0.5f);
    int sy = static_cast<int>(DATA::g_PoopGamePlayer.fY - DATA::g_PoopGamePlayer.fSize + 0.5f);
    int ex = static_cast<int>(DATA::g_PoopGamePlayer.fX + DATA::g_PoopGamePlayer.fSize + 0.5f);
    int ey = static_cast<int>(DATA::g_PoopGamePlayer.fY + DATA::g_PoopGamePlayer.fSize + 0.5f);
    int cx = static_cast<int>(DATA::g_PoopGamePlayer.fX + 0.5f);
    int cy = static_cast<int>(DATA::g_PoopGamePlayer.fY + 0.5f);

    for(int y = sy; y<=ey; y++)
    {
        float fDisY = (float)(y - cy);
        float fDisYY = fDisY * fDisY;
        for(int x = sx; x<=ex; x++)
        {
            float fDisX = (float)(x - cx);
            float fDisXX = fDisX * fDisX;
            float fDisXY = sqrt(fDisYY + fDisXX);

            float fPower = 1.f - fDisXY / DATA::g_PoopGamePlayer.fSize;
            int iPower = (int)(255.f * fPower);
            if(255 < iPower)
                iPower = 255;
            else if(iPower <= 0)
                continue;

            DrawPixel(x, y, TPixelColor(0, iPower, iPower));
        }
    }
}
void MainProcess(BOOL bPause)
{
    if(!bPause)
        FrameProcess();

    RenderProcess();

    if(!bPause)
    {
        AutoWind();
        AutoSnowIncrement();
    }
}
void AutoWind()
{
    if(!DATA::g_flagAutoEnvironment)
        return;
    if(--DATA::g_AutoWind_NextTick != 0)
        return;

    DATA::g_AutoWind_NextTick = (int)(DATA::gc_fps * 0.1f);
    
    //if(DATA::g_Wind == 0 && 85 > rand() % 100)
    //    return; // 무풍인 경우 85%확률로 유지
    float fWindNormal = (float)DATA::g_Wind / DATA::gc_maxWindSpeed;
    int chance = (int)(100.f * abs(fWindNormal));
    chance = min(chance, 10);
    if(chance < rand() % 100)
        return;

    // 방향 결정
    int newDirection = -1 + rand()%3;
    if(newDirection == 0)
        return;
    if(0 != DATA::g_AutoWind_ModifyPower)
    {
        int prevDirection = ((DATA::g_AutoWind_ModifyPower < 0)? -1 : +1);
        if(0 == newDirection + prevDirection)// 이전방향 새로운방향이 다르다면
        {
            if(0 != rand()%(abs(DATA::g_AutoWind_ModifyPower)+1))
                return;
            DATA::g_AutoWind_ModifyPower = 0;
        }
        else if(0 != rand() % (min(abs(DATA::g_Wind), abs(DATA::g_AutoWind_ModifyPower))+1))
        {
            // 연속 변화 누적이 클 수록 확률은 적어진다
            return;
        }
    }
    if(abs(DATA::g_AutoWind_ModifyPower) < DATA::gc_maxWindSpeed)
        DATA::g_AutoWind_ModifyPower += newDirection;

    int tW = 1;
    DATA::g_Wind += (tW * newDirection);
    if(DATA::g_Wind < -DATA::gc_maxWindSpeed)
        DATA::g_Wind = -DATA::gc_maxWindSpeed;
    else if(DATA::g_Wind > DATA::gc_maxWindSpeed)
        DATA::g_Wind = DATA::gc_maxWindSpeed;
}
int GetRandomTime(int minSec, int maxSec)
{
    assert(maxSec > minSec);
    const int min = minSec;
    const int rnd = maxSec - minSec + 1;
    int r = min + rand() % rnd;
    return r;
}
void AutoSnowIncrement_Normal()
{
    if(DATA::g_stats_FPS == 0.f)
        return;

    DATA::g_AutoSnowIncrement_NextTime_Remaining -= (1.f / DATA::g_stats_FPS);
    if(0 < DATA::g_AutoSnowIncrement_NextTime_Remaining)
        return;
    DATA::g_AutoSnowIncrement_NextTime_Remaining = (float)GetRandomTime(2, 4);;


    // 증가 확률 결정 : 현재 fps 를 기준 fps와 비교하여 결정
    int up;
    if(DATA::g_stats_FPS >= DATA::gc_fps * 0.9) // 기준 fps 90% 이상
        up = 60;
    else if(DATA::g_stats_FPS < DATA::gc_fps * 0.8) // 기준 fps 80% 미만
        up = 40;
    else
        up = 50;

    if(up > rand() % 100)
        MaxUnitsIncrement();
    else
        MaxUnitsDecrement();
}
void AutoSnowIncrement_Benchmarking()
{
    // 벤치마킹 테스트 시간을 기준으로
    // 시간5% ~ 시간90%
    // 최소유닛 ~ 최대 유닛
    auto maxTime = DATA::gc_Benchmark_TimeConsumptionSec * DATA::g_Benchmark_Tick_per_Sec;
    auto perTime = DATA::g_Benchmark_TimeRemaining / (double)maxTime;
    perTime = 1. - perTime; // 1~0 -> 0~1

    const int nRange = DATA::gc_nSnowUnits *10 / 9 - DATA::gc_iStepUsingSnowUnits;
    
    int n;
    if(perTime<0.05)
        n = DATA::gc_iStepUsingSnowUnits;
    else if(0.9 < perTime)
        n = DATA::gc_nSnowUnits;
    else
    {
        perTime -= 0.05;
        n = DATA::gc_iStepUsingSnowUnits + (int)(nRange * perTime);
    }

    MaxUnitsSet(n);
}
void AutoSnowIncrement_PoopGame()
{

}
void AutoSnowIncrement()
{
    if(!DATA::g_flagAutoEnvironment)
        return;

    switch(DATA::g_flagMode)
    {
    case DATA::E_MODE::E_Benchmark:
        AutoSnowIncrement_Benchmarking();

        break;
    case DATA::E_MODE::E_Game_Poop:
        AutoSnowIncrement_PoopGame();
        break;

    default:
        AutoSnowIncrement_Normal();
    }
}
void Draw_from_Mouse()
{
    //if(0 != DATA::g_Speed)
    //    return;

    SHORT toggle = 0x80 & ::GetKeyState(VK_LBUTTON);
    if(!toggle)
        return;

    using namespace DATA;
    POINT pt;
    ::GetCursorPos(&pt);
    ::ScreenToClient(g_hwnd, &pt);
    if(pt.x < 0) return;
    if(pt.x >= gc_ScreenSizeWidth) return;
    if(pt.y < 0) return;
    if(pt.y >= gc_ScreenSizeHeight) return;

    const int brushRadius = g_BrushSize;
    const int R = 120;
    const int G = 60;
    const int B = 60;

    int xL = pt.x - brushRadius;
    xL += gc_maxHorizonWave;
    if(xL < 0)
        xL = 0;
    
    int xR = pt.x + brushRadius;
    xR += gc_maxHorizonWave;
    if(xR >= gc_MapSizeWidth)
        xR = gc_MapSizeWidth -1;

    int yT = pt.y - brushRadius;
    if(yT < 0)
        yT = 0;

    int yB = pt.y + brushRadius;
    if(yB >= gc_ScreenSizeHeight)
        yB = gc_ScreenSizeHeight - 1;

    for(int x=xL; x<=xR; x++)
    {
        for(int y=yT; y<=yB; y++)
        {
            if(x==xL || x==xR || y==yT || y==yB)
            {
                // OutSide
                if(g_Map[x][y].at == 0)
                    g_Map[x][y].at = -1;
                continue;
            }
            if(g_Map[x][y].at == 0 || g_Map[x][y].at == -1)
                ;
            else
                continue;

            g_Map[x][y].at = gc_AT_Object;
            g_Map[x][y].SetSnowColor(0);
            ::SetPixel(g_pBitmapBG, x, y, TPixelColor(R, G, B));
            if(g_Speed == 0)
            {
                ::SetPixel(g_hdc, x, y, RGB(R, G, B));
            }
        }
    }


}
