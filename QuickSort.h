#pragma once
/*----------------------------------------------------------------
/	[퀵소트: 비재귀 스레드동작]
/----------------------------------------------------------------
/
/   사용자 함수:
/       namespace UTIL에 들어있습니다.
/           void Sort<T>(...)
/           void Sort<T, Functor__Compare>(...)
/           void _Sort<T, Functor__Compare, TValue>(...)
/               - T                 : 반복자 또는 Value*
/               - Functor__Compare  : 비교를 위한 함수객체
/               - TValue            : 실제 Vaule형식
/
/	참조:
/       - 사용된 일부 타입 / 함수는 해당 헤더를 포함시키지 않았습니다.
/           필요 헤더는 직접 이 파일을 포함전에 적당한 시점에 포함해 주십시오.
/           (이 헤더에 포함시키지 마십시오.)
/               Window.h 또는 (WinDef.h, WinNT.h, WinBase.h)
/               Process.h
/               Assert.h
/       - 퀵소트시 Key의 위치
/           반복자의 opeartor + 연산이 지원되는 경우 시작~끝의 50%위치
/           그렇지 않다면 시작~끝의 25%위치로 설정 합니다.
/
/       ※ 주의: 파라미터의 Last데이터는 표준 컨테이너에서 말하는 End데이터와는 달리,
/                유효한 마지막 데이터를 요구합니다.
/                표준 컨테이너들은 범위를 넘길때 Begin, End로 넘기지만,
/                이 객체는 Begin, End-1을 필요로 합니다.
/
/       ※ 주의: 비교함수의 동작에 있어 다음을 만족해야 합니다.
/                타입T의 Value A가 있다고 가정할때,
/                (A < A)? 는 FALSE를 만족해야 합니다.
/
/       - 다음 연산의 사용가능을 만족해야 합니다. 필요시 T형에 대하여 정의가 필요합니다.
/           값 취득             : *T
/           값 대입             : T = T
/           다음 값으로 이동    : 전위 ++ 연산자
/           이전 값으로 이동    : 전위 -- 연산자
/           비교 연산           : == ( 값이 아닌 주소 )
/
/       - 스레드동작의 제한사항 : 프로세서가 2개 이상이고, 데이터가 충분히 많을때 작동합니다.
/----------------------------------------------------------------
/   테스트 결과                     결과비(std::sort)
/   --- 데이터 10000개 이하 ---
/       T*, std::vector::iterator
/           기  본 : 소모시간 + 약10%
/           스레드 : 데이터가 충분히 많을때부터 빠름(데이터가 많아질수록 이득은 커짐)
/       std::list::iterator
/           기  본 : 소모시간 - 약25%
/           스레드 : 데이터가 충분히 많을때부터 빠름(데이터가 많아질수록 이득은 커짐)
/   --- 데이터 100만개 테스트 ---
/       T*, std::vector::iterator       std::sort(0.1554)
/           기  본 :    0.1696      소모시간 + 약9%
/           스레드 :    0.0983      소모시간 - 약36%
/       std::list::iterator             std::sort(1.5681)
/           기  본 :    0.5604      소모시간 - 약64%
/           스레드 :    0.3642      소모시간 - 약76%
/       ※ 임의의 타입T(반복자는) std::list와 같은 결과
/       ※ 데이터가 이미 정렬되었거나 역정렬된경우 std::sort에 비해 전체적으로 약세
/----------------------------------------------------------------
/
/	작성자: lastpenguin83@gmail.com
/	작성일: 07-10-16(화)
/	수정일: 09-07-08(수)
/           - 리펙토링
/               - 함수객체를 받아 사용가능하도록 수정
/               - List구조를 사용가능 하도록 수정
/               - 필요시 스레드 동작
/           09-07-09(목)
/           - Stack을 확장중 Stack 복사방식을 두가지 방법으로 분류
/             1. memcpy     : 일반적인 경우
/             2. 직접대입   : T가 참조카운트등의 영향이 있는 경우에 사용합니다.
/           - 디버깅 코드를 추가
/           09-08-26(수)
/           - 스레드동작관련 리펙토링(이제 두개의 스레드가 서로 상대가 처리하는 일이 없을때 데이터를 넘긴다)
/           13-09-21(토)
/           - _Set_KeyFocus, _GetCount 에 대하여, 기존 vector 외에 deque도 추가
/           15-02-14(토)
/           - vector / deque 의 내용이 수정되어 오류가 발생
/             std::_Vector_iterator<_Ty, _Alloc> 가 std::_Vector_iterator<_Myvec> 로 변경었음(deque 또한 마찬가지)
/             이에 맞게 수정함
----------------------------------------------------------------*/
#include <vector>
#include <deque>

namespace UTIL{


    /*----------------------------------------------------------------
    /       매크로 선언
    ----------------------------------------------------------------*/
    // 디버깅 매크로
  #if !defined(_DEF_QUICKSORT__DEBUGING)
    #if defined(_DEBUG)
      #define _DEF_QUICKSORT__DEBUGING     1
    #else
      #define _DEF_QUICKSORT__DEBUGING     0
    #endif
  #endif

  #if !defined(_Assert)
    #if defined(_DEBUG) || _DEF_QUICKSORT__DEBUGING
      #define _Assert(expr)     assert(expr)
    #else
      #define _Assert(expr)     _noop
    #endif
  #endif

  #ifndef OUT
    #define OUT
  #endif

    /*----------------------------------------------------------------
    /       기본 비교 함수객체 정의
    ----------------------------------------------------------------*/
    template<typename T>
    struct TFunctor_Less__Default
    {
        inline BOOL operator () (const T& _Left, const T& _Right) const
        {
            return *_Left < *_Right;
        }
    };
    template<typename T>
    struct TFunctor_Greater__Default
    {
        inline BOOL operator () (const T& _Left, const T& _Right) const
        {
            return *_Left > *_Right;
        }
    };

    /*----------------------------------------------------------------
    /       QuickSort 함수 - 기본 사용
    /   _First                  : 최초 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Last                   : 유효한 마지막 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Count                  : 데이터 개수
    /   _bUsingThread           : 스레드를 사용
    /   _bInverse               : 내림차순 정렬
    ----------------------------------------------------------------*/
    template<typename T>
    inline void Sort(T _First, T _Last
        , size_t _Count
        , BOOL   _bUsingThread
        , BOOL   _bInverse = FALSE
        )
    {
        if(!_bInverse)
            _Sort< T, TFunctor_Less__Default<T> >(_First, _Last, _Count, _bUsingThread, *_First);
        else
            _Sort< T, TFunctor_Greater__Default<T> >(_First, _Last, _Count, _bUsingThread, *_First);
    }

    /*----------------------------------------------------------------
    /       QuickSort 함수 - 비교 함수객체 지정 사용
    /   _First                  : 최초 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Last                   : 유효한 마지막 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Count                  : 데이터 개수
    /   _bUsingThread           : 스레드를 사용
    /   Functor__Compare        : 함수객체 타입
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare>
    inline void Sort(T _First, T _Last
        , size_t           _Count
        , BOOL             _bUsingThread
        , Functor__Compare
        )
    {
        _Sort<T, Functor__Compare>(_First, _Last, _Count, _bUsingThread, *_First);
    }

    /*----------------------------------------------------------------
    /       QuickSort 함수
    /   _First                  : 최초 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Last                   : 유효한 마지막 데이터의 주소 또는 데이터를 가리키는 반복자
    /   _Count                  : 데이터 개수
    /   _bUsingThread           : 스레드를 사용
    /   <TValue>                : T가 가르키는 실제값
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    inline void _Sort(T _First, T _Last
        , size_t _Count
        , BOOL   _bUsingThread
        , TValue)
    {
        if(_First == _Last)
            return;

        CQuickSort<T, Functor__Compare, TValue> _SortObject;

        if(_bUsingThread)
            _SortObject.mFN_SortThread(_First, _Last, _Count);
        else
            _SortObject.mFN_Sort(_First, _Last, _Count);
    }

    template<typename T, class Functor__Compare, typename TValue>
    class CQuickSort{
    public:
        /*----------------------------------------------------------------
        /       내부 타입 정의
        ----------------------------------------------------------------*/
        // 비재귀 동작에 필요한 스택단위
        struct TNonRecursive{
            T           BlockFirst;
            T           BlockLast;
            size_t      _Count;
        };

        // Thread 동작에 필요한 파라미터
        struct TThreadRun_Param{
            volatile BOOL m_bIdle;

            T       m_BlockFirst;
            T       m_BlockLast;
            size_t  m_Count;
        };

    public:
        /*----------------------------------------------------------------
        /       생성자
        ----------------------------------------------------------------*/
        CQuickSort();

    private:
        /*----------------------------------------------------------------
        /       비공개 데이터 - 설정 정보
        ----------------------------------------------------------------*/
        static const size_t _ISort_SmallScale_Max;  // 소규모정렬을 실행하기위한 데이터 크기 제한
        static const size_t _IMinimum_ThredBegin;   // 스레드소트시 최소한의 데이터 개수
        Functor__Compare    _Compare;               // 함수객체

    public:
        /*----------------------------------------------------------------
        /       메소드
        ----------------------------------------------------------------*/
        void mFN_Sort(T _First, T _Last, size_t _Count);
        void mFN_SortThread(T _First, T _Last, size_t _Count);

    private:
        void _Sort_SmallScale(T _First, T _Last);
        void _Sort_Thread(TThreadRun_Param* pThis, TThreadRun_Param* pParam);

        // Sort - Thread 함수
        static unsigned __stdcall ThreadFunction_Sort(void* _pParam);

        // 비재귀를 위한 스택버퍼를 확장한다.
        void _ExpansionStackBuffer( TNonRecursive*& pBuffer, size_t& nStackSize, size_t& nStackMaximum);
        // 대략적인 깊이를 테스트한다.
        size_t _TestDepth(size_t _Count);

    private:
        /*----------------------------------------------------------------
        /       인라인 확장 함수
        ----------------------------------------------------------------*/
        inline void _Swap(T& _A, T& _B)
        {
            TValue Temp = *_A;
            *_A         =   *_B;
            *_B         =   Temp;
        }
        inline void _Swap(T& _A, T& _B, T& _C)
        {
            TValue Temp = *_A;
            *_A         =   *_B;
            *_B         =   *_C;
            *_C         =   Temp;
        }

        // 1개의 키를 기준으로 퀵소트 1회
        void _Sort_Quick(T BlockFirst, T BlockLast, size_t _Count
            , T& OUT L_First, T& OUT L_Last, size_t& OUT L_Count
            , T& OUT R_First, T& OUT R_Last, size_t& OUT R_Count);

        // Key의 위치를 설정
        template<class _Myvec>
        inline void _Set_KeyFocus(std::_Vector_iterator<_Myvec>* OUT pIter, size_t _Count);
        template<class _Mydeque>
        inline void _Set_KeyFocus(std::_Deque_iterator<_Mydeque>* OUT pIter, size_t _Count);
        template<typename _Ty>
        inline void _Set_KeyFocus(_Ty** OUT ppData, size_t _Count);
        template<typename _Ty>
        inline void _Set_KeyFocus(_Ty* OUT pIter, size_t _Count);

        // 구간의 크기를 얻는다
        template<class _Myvec>
        inline size_t _GetCount(std::_Vector_iterator<_Myvec> First, std::_Vector_iterator<_Myvec> Last) const;
        template<class _Mydeque>
        inline size_t _GetCount(std::_Deque_iterator<_Mydeque> First, std::_Deque_iterator<_Mydeque> Last) const;
        template<typename _Ty>
        inline size_t _GetCount(_Ty* First, _Ty* Last) const;
        template<typename _Ty>
        inline size_t _GetCount(_Ty First, _Ty Last) const;
    };





    // 소규모정렬을 실행하기위한 데이터 크기 제한
    template<typename T, class Functor__Compare, typename TValue>
    const size_t CQuickSort<T, Functor__Compare, TValue>::_ISort_SmallScale_Max = 4;

    // 스레드소트시 최소한의 데이터 개수
    //  2의 배수로 계속 늘려가며 테스트 했는데, 본인의 컴퓨터에서는
    //  32768에서 스레드 소트가 기본소트를 앞질렀다.
    //  환경에 따라 변경해주면 좋을듯하다.
    template<typename T, class Functor__Compare, typename TValue>
    const size_t CQuickSort<T, Functor__Compare, TValue>::_IMinimum_ThredBegin = 32768;


    /*----------------------------------------------------------------
    /       생성자
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    CQuickSort<T, Functor__Compare, TValue>::CQuickSort()
    {
    }

    /*----------------------------------------------------------------
    /       Sort - 비 스레드
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::mFN_Sort(T _First, T _Last, size_t _Count)
    {
        if(_First == _Last)
            return;

        if(_Count <= 0)
            _Count = _GetCount(_First, _Last);
      #if _DEF_QUICKSORT__DEBUGING
        else { _Assert(_Count == _GetCount(_First, _Last)); }
      #endif

        if(_Count <= _ISort_SmallScale_Max)
        {
            _Sort_SmallScale(_First, _Last);
            return;
        }

        /*----------------------------------------------------------------
        /       재귀를 없애고 처리하지 못한정보는 스택에 잠시 담는다.
        /       1개의 블록단위로 처리한다.
        /       처리의 우선순위는 작은 것을 먼저
        ----------------------------------------------------------------*/
        TNonRecursive* pNonRecursiveStack   = NULL;
        size_t nStackMaximum = _TestDepth(_Count);
        size_t nStackSize    = 0;

        T   BlockFirst  = _First;
        T   BlockLast   = _Last;

        for(;;)
        {
          #if _DEF_QUICKSORT__DEBUGING
            _Assert(_ISort_SmallScale_Max < _Count);
          #endif
            T L_First, L_Last;
            T R_First, R_Last;
            size_t L_Count, R_Count;
            
            // 1개의 키를 기준으로 퀵소트 1회
            _Sort_Quick(BlockFirst, BlockLast, _Count
                , L_First, L_Last, L_Count
                , R_First, R_Last, R_Count);

            if(0 < L_Count && L_Count <= _ISort_SmallScale_Max)
            {
                _Sort_SmallScale(L_First, L_Last);
                L_Count = 0;
            }
            if(0 < R_Count && R_Count <= _ISort_SmallScale_Max)
            {
                _Sort_SmallScale(R_First, R_Last);
                R_Count = 0;
            }

            if(0 < L_Count && 0 < R_Count)
            {
                // 스택확장
                if(!pNonRecursiveStack || nStackMaximum <= nStackSize)
                    _ExpansionStackBuffer(pNonRecursiveStack, nStackSize, nStackMaximum);

                // 우측 구간을 스택에 보관한다.
                pNonRecursiveStack[nStackSize].BlockFirst   = R_First;
                pNonRecursiveStack[nStackSize].BlockLast    = R_Last;
                pNonRecursiveStack[nStackSize]._Count       = R_Count;
                ++nStackSize;

                // 작업구간을 좌측 구간으로 설정
                BlockFirst = L_First;
                BlockLast  = L_Last;
                _Count     = L_Count;
            }
            else if(0 < L_Count)
            {
                // 작업구간을 좌측 구간으로 설정
                BlockFirst = L_First;
                BlockLast  = L_Last;
                _Count     = L_Count;
            }
            else if(0 < R_Count)
            {
                // 작업구간을 우측 구간으로 설정
                BlockFirst = R_First;
                BlockLast  = R_Last;
                _Count     = R_Count;
            }
            else
            {
                // 이 구간은 더이상 분할되지 않는다. 이전에 스택에 담은 구간을 처리한다.
                if(nStackSize == 0)     // 더이상 처리할 구간이 없다면 종료
                    break;

                // 스택에서 처리하지 않은 작업구간을 꺼낸다.
                --nStackSize;
                BlockFirst  = pNonRecursiveStack[nStackSize].BlockFirst;
                BlockLast   = pNonRecursiveStack[nStackSize].BlockLast;
                _Count      = pNonRecursiveStack[nStackSize]._Count;
            }
        }

        if(pNonRecursiveStack)
            delete [] pNonRecursiveStack;
    }

    /*----------------------------------------------------------------
    /       Sort - Thread(진입 전)
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::mFN_SortThread(T _First, T _Last, size_t _Count)
    {
        if(_First == _Last)
            return;
        if(_Count <= 0)
            _Count = _GetCount(_First, _Last);
      #if _DEF_QUICKSORT__DEBUGING
        else { _Assert(_Count == _GetCount(_First, _Last)); }
      #endif
        if(_Count <= _ISort_SmallScale_Max)
        {
            _Sort_SmallScale(_First, _Last);
            return;
        }

        SYSTEM_INFO _SysInfo;
        ::GetSystemInfo(&_SysInfo);
        if(_SysInfo.dwNumberOfProcessors < 2 || _Count <= _IMinimum_ThredBegin)
        {
            // 프로세서수가 2개 미만이라면, 일반적인 소트를 실행한다.
            mFN_Sort(_First, _Last, _Count);
            return;
        }



        // 0: Main, 1: Sub
        TThreadRun_Param ThreadParam[2];

        // 최초 주스레드부터 소트를 시작한다.
        //          스레드A                 스레드B
        // ( 만약 두개의 스레드가 둘다 대기중이라면 리턴 )
        // ( 자신의 일이 없다면 대기                     )
        //------ 이하 처리할 데이터를 받은 경우 ------
        // ( Key를 잡고 1개의 구간을 Sort                )
        // ( 분할된 구간이 1개라면 다시 위과정 반복      )
        // 이하 분할된 구간이 2개 인경우
        //      상대 스레드가 대기중 이라면, 쪼개진 구간중, 큰 구간을 전달한다.
        //      작은 구간은 현재 스레드가 처리한다.
        // 이유: 현재 스택에 처리해야할 데이터가 아직 더 남아 있을 수 있다.
        ThreadParam[0].m_bIdle = FALSE;
        ThreadParam[0].m_BlockFirst = _First;
        ThreadParam[0].m_BlockLast  = _Last;
        ThreadParam[0].m_Count      = _Count;
        
        ThreadParam[1].m_bIdle = TRUE;

        // 보조 스레드를 생성한다.
        unsigned    _ThreadID;
        HANDLE hThread = (HANDLE)::_beginthreadex(NULL, 0, ThreadFunction_Sort, ThreadParam, 0, &_ThreadID);
        if(hThread == 0)
        {
            // 스레드 생성이 실패하였다. 기본 소트를 실행한다.
            mFN_Sort(_First, _Last, _Count);
            return;
        }

        _Sort_Thread(&ThreadParam[0], &ThreadParam[1]);

        // 보조 스레드를 기다린다.
        ::WaitForSingleObject(hThread, INFINITE);
        ::CloseHandle(hThread);
    }

    /*----------------------------------------------------------------
    /       Sort - Small Scale
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::_Sort_SmallScale(T _First, T _Last)
    {
        // 선택 정렬
        while( !(_First == _Last) )
        {
            // 2 <= (_First ~ _Last)
            T Now = _First; // 탐색위치
            T Sel = _First; // 선택
            for(;;)
            {
                ++Now;

                if( _Compare(Now , Sel) )
                    Sel = Now;

                if(Now == _Last)
                    break;
            }
            
            if( !(_First == Sel) )
                _Swap(_First, Sel);

            ++_First;
        }
    }

    /*----------------------------------------------------------------
    /       Sort - Thread
    ----------------------------------------------------------------*/
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::_Sort_Thread(TThreadRun_Param* pThis, TThreadRun_Param* pParam)
    {
        // 데이터와 데이터 크기 신뢰할수 있는것으로 본다.

        /*----------------------------------------------------------------
        /       재귀를 없애고 처리하지 못한정보는 스택에 잠시 담는다.
        /       1개의 블록단위로 처리한다.
        /       처리의 우선순위는 작은 것을 먼저
        ----------------------------------------------------------------*/
        TNonRecursive* pNonRecursiveStack   = NULL;
        size_t nStackMaximum = 0;
        size_t nStackSize    = 0;

        for(;;)
        {
            if(pThis->m_bIdle)
            {
                if(pParam->m_bIdle) // 두 스레드가 모두 대기중이라면 소트는 모두 끝난것이다.
                {
                    // 그 사이에 이스레드에게 데이터를 넘기고 완료했을 가능성도 있다.
                    //  현재 스레드를 1회더 테스트
                    if(pThis->m_bIdle)
                        break;
                }
                // 처리할 데이터가 넘어올때까지 대기한다.
                continue;
            }

            // 처리할 데이터를 받았다.
            T BlockFirst  = pThis->m_BlockFirst;
            T BlockLast   = pThis->m_BlockLast;
            size_t _Count = pThis->m_Count;

            // 스택의 최대 크기가 정해지지 않았다면 지금 정한다.
            if(nStackMaximum == 0)
                nStackMaximum = _TestDepth(_Count);

            for(;;)
            {
              #if _DEF_QUICKSORT__DEBUGING
                _Assert(_ISort_SmallScale_Max < _Count);
              #endif
                T L_First, L_Last;
                T R_First, R_Last;
                size_t L_Count, R_Count;

                // 1개의 키를 기준으로 퀵소트 1회
                _Sort_Quick(BlockFirst, BlockLast, _Count
                    , L_First, L_Last, L_Count
                    , R_First, R_Last, R_Count);

                if(0 < L_Count && L_Count <= _ISort_SmallScale_Max)
                {
                    _Sort_SmallScale(L_First, L_Last);
                    L_Count = 0;
                }
                if(0 < R_Count && R_Count <= _ISort_SmallScale_Max)
                {
                    _Sort_SmallScale(R_First, R_Last);
                    R_Count = 0;
                }

                if(0 < L_Count && 0 < R_Count)
                {
                    // 두 두간의 크기를 비교하여 작은 것을 처리하고
                    // 큰것을 다른 스레드에게 넘기거나, 스택에 담는다.
                    T Big_First, Big_Last;
                    T Small_First, Small_Last;
                    size_t Big_Count, Small_Count;
                    if(L_Count < R_Count)
                    {
                        Big_First   = R_First;
                        Big_Last    = R_Last;
                        Big_Count   = R_Count;
                        Small_First = L_First;
                        Small_Last  = L_Last;
                        Small_Count = L_Count;
                    }
                    else
                    {
                        Big_First   = L_First;
                        Big_Last    = L_Last;
                        Big_Count   = L_Count;
                        Small_First = R_First;
                        Small_Last  = R_Last;
                        Small_Count = R_Count;
                    }

                    if(pParam->m_bIdle)
                    {
                        // 다른 스레드가 바쁘지 않다면 데이터를 넘긴다.
                        pParam->m_BlockFirst = Big_First;
                        pParam->m_BlockLast  = Big_Last;
                        pParam->m_Count      = Big_Count;
                        pParam->m_bIdle      = FALSE;
                    }
                    else
                    {
                        // 스택에 담아둔다.

                        // 스택확장
                        if(!pNonRecursiveStack || nStackMaximum <= nStackSize)
                            _ExpansionStackBuffer(pNonRecursiveStack, nStackSize, nStackMaximum);

                        pNonRecursiveStack[nStackSize].BlockFirst   = Big_First;
                        pNonRecursiveStack[nStackSize].BlockLast    = Big_Last;
                        pNonRecursiveStack[nStackSize]._Count       = Big_Count;
                        ++nStackSize;
                    }

                    // 작업구간을 좌측 구간으로 설정
                    BlockFirst = Small_First;
                    BlockLast  = Small_Last;
                    _Count     = Small_Count;
                }
                else if(0 < L_Count)
                {
                    // 작업구간을 좌측 구간으로 설정
                    BlockFirst = L_First;
                    BlockLast  = L_Last;
                    _Count     = L_Count;
                }
                else if(0 < R_Count)
                {
                    // 작업구간을 우측 구간으로 설정
                    BlockFirst = R_First;
                    BlockLast  = R_Last;
                    _Count     = R_Count;
                }
                else
                {
                    // 이 구간은 더이상 분할되지 않는다. 이전에 스택에 담은 구간을 처리한다.
                    if(nStackSize == 0)
                    {
                        // 더이상 처리할 구간이 없다면 이 스레드는 대기상태에 들어간다.
                        pThis->m_bIdle = TRUE;
                        break;
                    }

                    // 스택에서 처리하지 않은 작업구간을 꺼낸다.
                    --nStackSize;
                    BlockFirst  = pNonRecursiveStack[nStackSize].BlockFirst;
                    BlockLast   = pNonRecursiveStack[nStackSize].BlockLast;
                    _Count      = pNonRecursiveStack[nStackSize]._Count;
                }
            }
        }

        if(pNonRecursiveStack)
            delete [] pNonRecursiveStack;
    }

    // Sort - Thread 함수
    template<typename T, class Functor__Compare, typename TValue>
    unsigned __stdcall CQuickSort<T, Functor__Compare, TValue>::ThreadFunction_Sort(void* _pParam)
    {
        TThreadRun_Param* pThreadParamMain = reinterpret_cast<TThreadRun_Param*>(_pParam);
        TThreadRun_Param* pThreadParamSub  = pThreadParamMain + 1;

        CQuickSort<T, Functor__Compare, TValue>    _SortObject;
        _SortObject._Sort_Thread(pThreadParamSub, pThreadParamMain);

        return 0;
    }

    // 비재귀를 위한 스택버퍼를 확장한다.
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::_ExpansionStackBuffer(TNonRecursive*& pBuffer, size_t& nStackSize, size_t& nStackMaximum)
    {
        // 스택크기 확인
        if(!pBuffer)
        {
            // 스택을 만든다.
            pBuffer = new TNonRecursive[nStackMaximum];
        }
        else if(nStackMaximum <= nStackSize)
        {
            // 스택 확장
            nStackMaximum = nStackMaximum << 1;
            TNonRecursive* pNew_NonRecursiveStack = new TNonRecursive[nStackMaximum];

            // 이전 스택의 내용 복사
            // 관리되는 반복자의 경우가 있기때문에 대입연산이 일어나도록 합니다.
            // memcpy를 사용하지 않습니다.
            for(size_t CntStackData = 0; CntStackData < nStackSize; CntStackData++)
                pNew_NonRecursiveStack[CntStackData] = pBuffer[CntStackData];

            // 이전 스택 삭제
            delete [] pBuffer;
            pBuffer = pNew_NonRecursiveStack;
        }
    }

    // 대략적인 깊이를 테스트한다.
    template<typename T, class Functor__Compare, typename TValue>
    size_t CQuickSort<T, Functor__Compare, TValue>::_TestDepth(size_t _Count)
    {
        size_t _Depth = 0;
        
        while(_ISort_SmallScale_Max < _Count)
        {
            _Count = _Count >> 1;
            _Depth += 2;
        }
        return _Depth;
    }

    // 1개의 키를 기준으로 퀵소트 1회
    template<typename T, class Functor__Compare, typename TValue>
    void CQuickSort<T, Functor__Compare, TValue>::_Sort_Quick(T BlockFirst, T BlockLast, size_t _Count
        , T& OUT L_First, T& OUT L_Last, size_t& OUT L_Count
        , T& OUT R_First, T& OUT R_Last, size_t& OUT R_Count)
    {
        T L = BlockFirst;
        T R = BlockLast;
        T K = L;

        // Key의 위치를 설정
        _Set_KeyFocus(&K, _Count);

        BOOL L_is_K = FALSE;
        BOOL R_is_K = FALSE;
        for(;;)
        {
            while(!L_is_K)
            {
                // Find L > K
                while(!_Compare(K, L))
                {
                    if(L == K)
                    {
                        L_is_K = TRUE;
                        break;
                    }
                    ++L;
                }

                if(!R_is_K)
                    break;
                else if(!L_is_K)
                {
                    //만약 R이 K에 정체되어 있다면 R만 순회하면 된다.

                    // K == R
                    // Before [ L][ T][KR][  ][  ]
                    // After  [ T][KR][ L][  ][  ]
                    --K;
                    if(L == K)
                    {
                        _Swap(K, R);
                        L_is_K = TRUE;
                        break;
                    }

                    _Swap(K, R, L);
                    R = K;
                }
            }
            
            while(!R_is_K)
            {
                // Find K > R
                while(!_Compare(R, K))
                {
                    if(K == R)
                    {
                        R_is_K = TRUE;
                        break;
                    }
                    --R;
                }
                if(!L_is_K)
                    break;
                else if(!R_is_K)
                {
                    //만약 L이 K에 정체되어 있다면 R만 순회하면 된다.

                    // L == K
                    // Before [  ][  ][LK][ T][ R]
                    // After  [  ][  ][ R][LK][ T]
                    ++K;
                    if(K == R)
                    {
                        _Swap(L, K);
                        R_is_K = TRUE;
                        break;
                    }

                    _Swap(K, L, R);
                    L = K;
                }
            }
            
            // 교환
            if(L_is_K)
            {
                if(R_is_K)
                {
                    // L == K == R
                    break;
                }
                else
                {
                    // L == K
                    // Before [  ][  ][LK][ T][ R]
                    // After  [  ][  ][ R][LK][ T]
                    ++K;
                    if(K == R)
                    {
                        _Swap(L, K);
                        break;
                    }

                    _Swap(K, L, R);
                    L = K;
                }
            }
            else if(R_is_K)
            {
                // K == R
                // Before [ L][ T][KR][  ][  ]
                // After  [ T][KR][ L][  ][  ]
                --K;
                if(L == K)
                {
                    _Swap(K, R);
                    break;
                }

                _Swap(K, R, L);
                R = K;
            }
            else
            {
                // Before [ L][  ][ K][  ][ R]
                // After  [ R][  ][ K][  ][ L]
                _Swap(L, R);
                ++L;
                --R;
            }
        }
        
        if( !(K == BlockFirst) )
        {
            L_First = BlockFirst;
            L_Last  = K;
            --L_Last;
            L_Count = _GetCount(L_First, L_Last);
        }
        else
        {
            L_Count = 0;
        }

        if( !(K == BlockLast) )
        {
            R_First = K;
            ++R_First;
            R_Last  = BlockLast;
            R_Count = (_Count - L_Count) - 1;
        }
        else
        {
            R_Count = 0;
        }
    }
    // Key의 위치를 설정 - std::vector<TValue>::iterator
    template<typename T, class Functor__Compare, typename TValue>
    template<class _Myvec>
    void CQuickSort<T, Functor__Compare, TValue>::_Set_KeyFocus(std::_Vector_iterator<_Myvec>* OUT pIter, size_t _Count)
    {
        // 시작점으로부터 50% 위치
        _Count /= 2;
        (*pIter) += _Count;
    }
    // Key의 위치를 설정 - std::deque<TValue>::iterator
    template<typename T, class Functor__Compare, typename TValue>
    template<class _Mydeque>
    void CQuickSort<T, Functor__Compare, TValue>::_Set_KeyFocus(std::_Deque_iterator<_Mydeque>* OUT pIter, size_t _Count)
    {
        // 시작점으로부터 50% 위치
        _Count /= 2;
        (*pIter) += _Count;
    }
    // Key의 위치를 설정 - 데이터 포인터
    template<typename T, class Functor__Compare, typename TValue>
    template<typename _Ty>
    void CQuickSort<T, Functor__Compare, TValue>::_Set_KeyFocus(_Ty** OUT ppData, size_t _Count)
    {
        // 시작점으로부터 50% 위치
        _Count /= 2;
        (*ppData) += _Count;
    }
    // Key의 위치를 설정 - 기타 반복자
    template<typename T, class Functor__Compare, typename TValue>
    template<typename _Ty>
    void CQuickSort<T, Functor__Compare, TValue>::_Set_KeyFocus(_Ty* OUT pIter, size_t _Count)
    {
        // 시작점으로부터 25% 위치
        // 리스트구조의 경우 반복자의 위치를 변경하는 비용이 크기때문에,
        // 적당히 25%정도만한다.
        _Count /= 4;
        for(size_t i=0; i<_Count; ++i)
            ++(*pIter);
    }

    // 구간의 크기를 얻는다 - std::vector<TValue>::iterator
    template<typename T, class Functor__Compare, typename TValue>
    template<class _Myvec>
    size_t CQuickSort<T, Functor__Compare, TValue>::_GetCount(std::_Vector_iterator<_Myvec> First
        , std::_Vector_iterator<_Myvec> Last) const
    {
        return (1 + Last - First);
    }
    // 구간의 크기를 얻는다 - std::deque<TValue>::iterator
    template<typename T, class Functor__Compare, typename TValue>
    template<class _Mydeque>
    size_t CQuickSort<T, Functor__Compare, TValue>::_GetCount(std::_Deque_iterator<_Mydeque> First
        , std::_Deque_iterator<_Mydeque> Last) const
    {
        return (1 + Last - First);
    }
    // 구간의 크기를 얻는다 - 데이터 포인터
    template<typename T, class Functor__Compare, typename TValue>
    template<typename _Ty>
    size_t CQuickSort<T, Functor__Compare, TValue>::_GetCount(_Ty* First, _Ty* Last) const
    {
        return (1 + Last - First);
    }
    // 구간의 크기를 얻는다 - 기타 반복자
    template<typename T, class Functor__Compare, typename TValue>
    template<typename _Ty>
    size_t CQuickSort<T, Functor__Compare, TValue>::_GetCount(_Ty First, _Ty Last) const
    {
        size_t _Count = 1;
        for(_Ty iter = First; !(iter == Last); ++iter)
            ++_Count;
        return _Count;
    }

};
