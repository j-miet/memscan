#pragma once
#include "intscanner.hpp"
#include "memblock.hpp"
#include "stringscanner.hpp"

/**
 * \brief Implements user interface for string/integer scanners + initializes process memory for reading/writing
 */
class Scanner
{
    public:
        void uiNewScan();

        StringScanner createStringScanner(Condition startCondition);
        IntScanner createIntScanner(Condition startCondition);

        int openStringUi(StringScanner stringScanner);
        //int openIntUi(IntScanner intScanner);

        const bool&      isString()       const { return m_isString; }
        const Condition& startCondition() const { return m_startCondition; }

    private:
        std::vector<MemBlock> m_scan;
        Condition m_startCondition;
        int64_t m_intVal;
        std::string m_strVal;
        bool m_isString;
            
        std::vector<MemBlock> createScan(int processId, int dataSize);
        int getMatchesCount(std::vector<MemBlock> mbScan);
        int stringToInt(std::string s);

        void uiPrintStringMatches(StringScanner strScanner, int size);
        void uiWriteString(StringScanner strScanner);
        /*
        void uiPrintIntMatches(IntScanner intScanner);
        void uiWriteInt(IntScanner intScanner)
        */
};