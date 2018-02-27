#ifndef PROCMEM_H
#define PROCMEM_H

#ifdef _MSC_VER
#pragma once
#endif

#include "stdafx.h"
#include <TlHelp32.h>  


struct Memory
{
	HANDLE HProcess;
	DWORD dwPID;
	bool ProcAttach(char* ProcName)
	{
		HANDLE pHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		PROCESSENTRY32 ProcEntry;
		ProcEntry.dwFlags = sizeof(ProcEntry);
		do
		{
			if (!strcmp(ProcEntry.szExeFile, ProcName))
			{
				this->dwPID = ProcEntry.th32ProcessID;
				CloseHandle(pHandle);
				this->HProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->dwPID);
				return true;
			}
		} while (Process32Next(pHandle, &ProcEntry));
		return false;
	}
	DWORD GetModule(LPSTR ModuleName)
	{
		HANDLE HModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, this->dwPID);
		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		do
			if (!strcmp(ModuleEntry.szModule, ModuleName))
			{
				CloseHandle(HModule);
				return (DWORD)ModuleEntry.modBaseAddr;
			}
		while (Module32Next(HModule, &ModuleEntry));
		return 0;
	}
	template<class T>
	T Read(DWORD dwAddress)
	{
		T cRead;
		ReadProcessMemory(this->HProcess, (LPVOID)dwAddress, &cRead, sizeof(T), NULL);
		return cRead;
	}
	template<class T>
	void Write(DWORD dwAddress, T i)
	{
		WriteProcessMemory(this->HProcess, (LPVOID)dwAddress, &i, sizeof(i), NULL);
	}
};

#endif