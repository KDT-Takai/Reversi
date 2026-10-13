#pragma once
// DxLib.h は Shift-JIS で書かれているため、/utf-8 下で出るコメント由来の C4828 を抑止して取り込む

#pragma warning(push)
#pragma warning(disable : 4828 4010)
#include <DxLib.h>
#pragma warning(pop)
