# process-tree-scout - A process tree management utility

A command-line utility designed for **Linux-based systems** that allows developers, system administrators, and DevOps engineers to inspect, control, and manage process trees with fine-grained control. It extends typical process monitoring capabilities by providing targeted operations on descendant processes, siblings, and grandchild processes in a process hierarchy.

Unlike standard tools like `ps` or `pstree`, this utility allows programmatic **signal propagation, descendant filtering, and real-time process tree analysis**, making it ideal for complex systems, automated process management, and debugging production workloads.

---

## Features

- **Tree-aware process querying**
  - Verify whether a process belongs to a specific process tree.
  - Retrieve PID and PPID information for any process in a hierarchy.

- **Signal operations on process trees**
  - `-dx`: Kill all descendants of a root process (SIGKILL)
  - `-dt`: Stop all descendants (SIGSTOP)
  - `-dc`: Resume paused descendants (SIGCONT)
  - `-rp`: Kill a specific process

- **Hierarchy analysis and reporting**
  - `-nd`: List all **non-direct descendants** of a process
  - `-dd`: List all **immediate/direct descendants**
  - `-sb`: List all **sibling processes**
  - `-bz`, `-zd`, `-od`: List **defunct, orphan, or zombie processes** in the tree
  - `-gc`: List all **grandchildren** of a process
  - `-sz`, `-so`: Check **defunct or orphan status** of a process

- **Robust error handling**
  - Ensures operations are only performed if the target process belongs to the specified process tree.
  - Gracefully reports when no matching processes are found.

---

## Compile

```bash
gcc process.c -o prc
```
---

## Synopsis

```bash
./prc [Option] [root_process] [process_id]
```
---

## Example

```bash
./prc -zd 1004 1005
```

Listing descendant zombie processes of the process with PID=1005, rooted at process with PID=1004.