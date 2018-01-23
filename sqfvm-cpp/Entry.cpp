#include "full.h"


int main(int argc, char** argv)
{
	sqf::commandmap::get().init();
	auto vm = sqf::virtualmachine();

	vm.out() << std::endl << L"ASSEMBLY" << std::endl;
	vm.parse_assembly(L"callnullar ntest;");
	vm.execute();
	
	vm.out() << std::endl << L"SQF" << std::endl;
	vm.parse_sqf(L"1 + 1 - 2;0;1 + 2 * 3 - 1; 6");
	vm.execute();

	sqf::commandmap::get().uninit();
	system("pause");
}

/*
    0       4         0       4    0
[1, 2, 3] select ([0, 1, 2] select 2);

                  [0, 1, 2]        2
[1, 2, 3]        (          select  )
          select                     ;
                       [0, 1]
                 count          1
[1, 2, 3]                     +
          select                     ;

- (...) wie konstanten wert behandeln
- (...) rekursiv abarbeiten
- array mit index f�hren

*/