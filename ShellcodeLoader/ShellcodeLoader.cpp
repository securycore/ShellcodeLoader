// ShellcodeLoader.cpp: define el punto de entrada de la aplicación de consola.
//

#include "stdafx.h"

using namespace std;

typedef struct {
	int offset;
	bool nopause;
	bool debug;
	int preferredAddress;
	char* file;
} ProgramArgs, *pProgramArgs;

bool parse_args(pProgramArgs args, const char* argv[], int argc) {
	if (argc < 2) {
		printf("[-] Too few required arguments passed to ShellcodeLoader. No file specified!\n");
		return false;
	}
	args->file = (char*)argv[1];

	if (IsDebuggerPresent()) {
		args->debug = true;
		args->nopause = true;
	}

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--entrypoint") == 0 || strcmp(argv[i], "-e") == 0) {
			args->offset = strtol(argv[++i], NULL, 16);
		}
		else if (strcmp(argv[i], "--run") == 0 || strcmp(argv[i], "-r") == 0) {
			args->nopause = true;
		}
		else if (strcmp(argv[i], "--breakpoint") == 0 || strcmp(argv[i], "-b") == 0) {
			args->debug = true;
			args->nopause = true;
		}
		else if (strcmp(argv[i], "--address") == 0 || strcmp(argv[i], "-a") == 0) {
			args->preferredAddress = strtol(argv[++i], NULL, 16);
		}
		else {
			printf("[!] Warning: Unknown arg: %s\n", argv[i]);
		}
	}
	return true;
}

int main(int argc, const char** argv)
{
	ProgramArgs args = { 0 };
	char* printDirection;
	FUNC code;
	LPVOID memory = 0;

	//Parse args
	if (!parse_args(&args, argv, argc)) {
		return -1;
	};

	// let's open the file
	cout << "Welcome to ShellcodeLoader!" << endl;
	cout << "[+] Opening file: " << args.file << endl;
	HANDLE handle = CreateFileA(args.file, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (handle == INVALID_HANDLE_VALUE) {
		cout << "[-] File doesn't exist" << endl;
		return 2;
	}

	// get the file size
	cout << "[+] Getting file size" << endl;
	SIZE_T size = GetFileSize(handle, 0);

	if (size <= 0) {
		cout << "[-] Error reading size of file" << endl;
		return 3;
	}

	// let's try to allocate memory in the specified address
	if (args.preferredAddress != NULL) {
		cout << "[+] Allocating space in preferred address: ";
		printf("0x%X", args.preferredAddress);
		cout << endl;

		memory = VirtualAlloc((LPVOID)args.preferredAddress, size, 0x3000, 0x40);
		if (memory == NULL) {
			cout << "[-] Failed! Trying in another address..." << endl;
			memory = VirtualAlloc(0, size, 0x3000, 0x40);
		}
	}
	else {
		cout << "[+] Allocating space in memory" << endl;
		memory = VirtualAlloc(0, size, 0x3000, 0x40);
	}

	code = (FUNC)memory;
	if (code == NULL) {
		cout << "[-] Error allocating space" << endl;
		return 4;
	}

	// read the file into memory
	cout << "[+] Reading file into buffer" << endl;
	DWORD bytesRead;
	bool read = ReadFile(handle, code, size, &bytesRead, 0);

	if (read == NULL) {
		cout << "[-] Error reading file" << endl;
		return 5;
	}

	// close the handle to the file
	CloseHandle(handle);
	
	// calc the entrypoint to the shellcode
	if (args.offset != NULL) {
		printDirection = reinterpret_cast<char*>(code);
		printDirection += args.offset;
		code = reinterpret_cast<FUNC>(printDirection);
	}

	cout << "[+] Entry point will be at address: ";
	printf("0x%X", (unsigned int)code);
	cout << endl;
	
	// stop debugger 
	if (!args.nopause) {
		cout << "[+] Set a breakpoint to the Entry Point manually. Press any key to continue to the shellcode." << endl; 
		getchar();
	}

	// jump to shellcode
	cout << "[+] JUMP AROUND!" << endl;
	executeShellcode(args.debug, code);

	// free the memory allocated for the shellcode
	VirtualFree(memory, size, MEM_RELEASE);

	cout << "[+] Execution finished. Press any key to quit" << endl;
	getchar();

    return 0;
}

int filter(unsigned int code, struct _EXCEPTION_POINTERS *ep) {
	cout << "[!] Unhandled exception was generated during shellcode execution" << endl;
	return EXCEPTION_EXECUTE_HANDLER;
}

void executeShellcode(bool debugging, FUNC code) {
	__try {
		if (debugging) {
			__debugbreak();
		}
		code();
	}
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
	}
}