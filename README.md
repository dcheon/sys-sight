# SysSight

Linux x86-64에서 `ptrace(PTRACE_SYSCALL)` 기반으로 syscall entry/exit 이벤트를 수집하고, 파일 디스크립터 및 가상 메모리 상태를 순수 reducer로 재구성하는 C++20 프로젝트입니다.

## 현재 구현 범위

- C++20 + Modern CMake 프로젝트 스켈레톤
- `PTRACE_O_TRACESYSGOOD` 기반 단일 프로세스 syscall entry/exit 수집
- JSON Lines 출력
- ptrace와 독립적인 `FileDescriptorTable`, `VirtualMemoryMap`, `ProcessState`
- FD duplicate/close 및 VMA middle-unmap splitting 단위 테스트

현재 tracer는 Linux x86-64에서 실행해야 하며, Windows에서는 소스 구조와 테스트 코드만 편집할 수 있습니다.

## Linux 빌드

```bash
cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## 실행

```bash
./build/sys-sight /bin/echo
```

출력은 syscall 번호와 entry/exit phase를 담은 JSONL입니다. 다음 Phase에서 syscall decoder, `/proc` bootstrap/oracle, FD/VMA reducer 연결, `PTRACE_GET_SYSCALL_INFO`, seccomp 최적화를 추가합니다.
