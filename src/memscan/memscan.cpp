#include "scanner.hpp"
#include "intscanner.hpp"
#include "stringscanner.hpp"

int main() 
{
    Scanner scanner;
    int returnCode = 1;

    while (returnCode)
    {
        scanner.uiNewScan();
        if (scanner.isString())
        {
            StringScanner strScan = scanner.createStringScanner(scanner.startCondition());
            returnCode = scanner.openStringUi(strScan);
        }
        else
        {
            IntScanner intScan = scanner.createIntScanner(scanner.startCondition());
            //scanner.openIntUi(intScan);
        }
    }
    
    return 0;
}