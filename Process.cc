#include "lib/debug.h"
#include "Process.h"
#include "ProcMaps.h"
#include "ProcStatus.h"
#include "Migration.h"

int Process::load(pid_t n)
{
  pid = n;
  return proc_status.load(pid);
}

void Process::add_range(unsigned long start, unsigned long end)
{
  std::shared_ptr<Migration> p;

  p = std::make_shared<Migration>(pid);
  p->set_va_range(start, end);
  idle_ranges.push_back(p);

  printdd("pid=%d add_range %lx-%lx=%lx\n", pid, start, end, end - start);
}

int Process::split_ranges(unsigned long max_bytes)
{
  unsigned long rss_anon = proc_status.get_number("RssAnon") << 10;

  if (rss_anon <= 0)
    return 0;

  if (rss_anon < max_bytes) {
    add_range(0, TASK_SIZE_MAX);
    return 0;
  }

  unsigned long sum = 0;
  unsigned long start = 0;
  unsigned long end;
  auto vmas = proc_maps.load(pid);

  for (auto& vma: vmas) {

    if (vma.start >= TASK_SIZE_MAX)
      continue;

    unsigned long vma_size = vma.end - vma.start;
    unsigned long offset;
    sum += vma_size;
    for (offset = max_bytes; offset < vma_size; offset += max_bytes) {
      end = (vma.start + offset) & ~(PMD_SIZE - 1);
      add_range(start, end);
      start = end;
      sum = 0;
    }

    if (sum > max_bytes) {
      end = vma.end;
      add_range(start, end);
      start = end;
      sum = 0;
    }
  }

  if (sum)
    add_range(start, TASK_SIZE_MAX);

  return 0;
}

int ProcessCollection::collect()
{
  int err;
 
  proccess_hash.clear();

  err = pids.collect();
  if (err)
    return err;

  for (pid_t pid: pids.get_pids())
  {
    std::shared_ptr<Process> p = std::make_shared<Process>();

    err = p->load(pid);
    if (err)
      continue;

    err = p->split_ranges(SPLIT_RANGE_SIZE);
    if (err)
      continue;

    proccess_hash[pid] = p;
  }

  return 0;
}
