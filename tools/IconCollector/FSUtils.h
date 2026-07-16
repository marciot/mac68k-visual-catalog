#pragma once

void replaceChars(char *str, char find, char replace);
OSErr FSpSubDirCreate (const FSSpecPtr parentDir, FSSpecPtr createdDir, Str63 name, ScriptCode scriptTag = smSystemScript);
Boolean FSpFileExists (const FSSpecPtr fsSpec);
Boolean FSpFileExistsCaseSensitive (const FSSpecPtr fsSpec, Boolean *caseInsensitiveMatch = 0);