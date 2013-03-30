// stdafx.h: включаемый файл для стандартных системных включаемых файлов
// или включаемых файлов для конкретного проекта, которые часто используются, но
// не часто изменяются
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


#ifdef _DEBUG
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC // enable generation of debug heap alloc map
//#define new new( _NORMAL_BLOCK, __FILE__, __LINE__) // redefine "new" to get file names in output
#endif


// TODO: Установите здесь ссылки на дополнительные заголовки, требующиеся для программы
