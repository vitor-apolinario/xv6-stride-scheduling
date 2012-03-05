#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "amd64.h"
#include "cpu.hh"
#include "wq.hh"
#include "kalloc.hh"

static wq *wq_;

int
wq_push(work *w)
{
  return wq_->push(w);
}

int
wq_trywork(void)
{
  return wq_->trywork();
}

void
wq_dump(void)
{
  return wq_->dump();
}

void
initwq(void)
{
  wq_ = new wq();
}

//
// wq
//
void*
wq::operator new(unsigned long nbytes)
{
  assert(nbytes == sizeof(wq));
  return ksalloc(slab_wq);
}

wq::wq(void)
{
  int i;

  for (i = 0; i < NCPU; i++)
    initlock(&q_[i].lock, "wq lock", LOCKSTAT_WQ);
}

void
wq::dump(void)
{
  int i;
  for (i = 0; i < NCPU; i++)
    cprintf("push %lu full %lu pop %lu steal %lu\n",
            stat_[i].push, stat_[i].full,
            stat_[i].pop, stat_[i].steal);
}

int
wq::push(work *w)
{
  int i;

  pushcli();
  i = q_->head;
  if ((i - q_->tail) == NSLOTS) {
    stat_->full++;
    popcli();
    return -1;
  }
  i = i & (NSLOTS-1);
  q_->w[i] = w;
  barrier();
  q_->head++;
  stat_->push++;
  popcli();
  return 0;
}

inline work*
wq::pop(int c)
{
  struct wqueue *q = &q_[c];
  work *w;
  int i;

  i = q->head;
  if ((i - q->tail) == 0)
    return 0;
  
  acquire(&q->lock);
  i = q->head;
  if ((i - q->tail) == 0) {
    release(&q->lock);
    return 0;
  }
  i = (i-1) & (NSLOTS-1);
  w = q->w[i];
  q->head--;
  release(&q->lock);

  stat_->pop++;
  return w;
}

inline work*
wq::steal(int c)
{
  struct wqueue *q = &q_[c];
  work *w;
  int i;

  if (tryacquire(&q->lock) == 0)
    return 0;
  i = q->tail;
  if ((i - q->head) == 0) {
    release(&q->lock);
    return 0;
  }
  i = i & (NSLOTS-1);
  w = q->w[i];
  q->tail++;
  release(&q->lock);

  stat_->steal++;
  return w;
}

int
wq::trywork(void)
{
  work *w;
  u64 i, k;

  // A "random" victim CPU
  k = rdtsc();

  w = pop(mycpuid());
  if (w != nullptr) {
    w->run();
    return 1;
  }

  for (i = 0; i < NCPU; i++) {
    u64 j = (i+k) % NCPU;

    if (j == mycpuid())
        continue;
    
    w = steal(j);
    if (w != nullptr) {
      w->run();
      return 1;
    }
  }

  return 0;
}

//
// cwork
//
void
cwork::run(void)
{
  void (*fn)(void*, void*, void*, void*, void*) = 
    (void(*)(void*,void*,void*,void*,void*))rip;
  fn(arg0, arg1, arg2, arg3, arg4);
  delete this;
}

void*
cwork::operator new(unsigned long nbytes)
{
  assert(nbytes == sizeof(cwork));
  return kmalloc(sizeof(cwork));
}

void*
cwork::operator new(unsigned long nbytes, cwork* buf)
{
  assert(nbytes == sizeof(cwork));
  return buf;
}

void
cwork::operator delete(void *p)
{
  kmfree(p, sizeof(cwork));
}