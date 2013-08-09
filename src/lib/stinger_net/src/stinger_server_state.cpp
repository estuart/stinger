#include "stinger_server_state.h"

extern "C" {
  #include "stinger_core/stinger_error.h"
  #include "stinger_core/x86_full_empty.h"
}

#include <algorithm>

using namespace gt::stinger;

template<typename T>
struct delete_functor
{
  void operator() (T * item)
  {
    delete item;
  }
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *
 * PRIVATE METHODS
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
StingerServerState::StingerServerState() : port(10101), convert_num_to_string(1), 
				    alg_lock(1), stream_lock(1), batch_lock(1), dep_lock(1), mon_lock(1)
{
  LOG_D("Initializing server state.");
}

StingerServerState::~StingerServerState()
{
  LOG_D("Entering StingerServerState destructor");
  std::for_each(algs.begin(), algs.end(), delete_functor<StingerAlgState>());
  std::for_each(streams.begin(), streams.end(), delete_functor<StingerStreamState>());
  LOG_D("Leaving StingerServerState destructor");
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ *
 * PUBLIC METHODS
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
StingerServerState &
StingerServerState::get_server_state()
{
  LOG_D("called");
  static StingerServerState self;
  return self;
}

/**
* @brief Get the current port number.
*
* @return The current port number.
*/
int
StingerServerState::get_port() 
{
  LOG_D_A("returning %ld", (long) port);
  return port;
}

/**
* @brief Sets the port to be used by the server. 
* 
* NOTE: Should only be called during initialization - i.e. when parsing
* options.  This will not change the port once the server is running.
*
* @param new_port The new port number (1 - 65535, but really 1024 ~ 35000 or so).
*
* @return The port number (old one if new is invalid).
*/
int
StingerServerState::set_port(int new_port)
{
  LOG_D_A("called with %ld", (long) new_port);
  if(new_port > 0 && new_port < 65535) {
    port = new_port;
  } else {
    LOG_W_A("New port number %ld is invalid. Keeping %ld", (long) new_port, (long) port);
  }
  return port;
}

/**
* @brief Indicates whether or not the server should convert stinger IDs to strings
* when applying a batch so that algorithms have the string information in the post
* processing stage.
*
* @return 0 - false, otherwise - true
*/
int
StingerServerState::convert_numbers_only_to_strings() {
  return convert_num_to_string;
}

/**
* @brief Indicates whether or not the server should convert stinger IDs to strings
* when applying a batch so that algorithms have the string information in the post
* processing stage.
*
* @param new_value 0 - Do not convert, otherwise - Convert
*
* @return The input value
*/
int
StingerServerState::set_convert_numbers_only_to_strings(int new_value) {
  return convert_num_to_string = new_value;
}

/**
* @brief Atomically enqueue a received batch.
*
* @param batch A pointer to the batch to be enqueued (DO NOT DELETE)
*/
void
StingerServerState::enqueue_batch(StingerBatch * batch)
{
  LOG_D_A("%p %ld insertions %ld deletions: Attempt to acquire lock", batch, (long) batch->insertions_size(), (long) batch->deletions_size());
  readfe((uint64_t *)&batch_lock);
  LOG_D_A("%p %ld insertions %ld deletions: Lock acquired, enqueueing", batch, (long) batch->insertions_size(), (long) batch->deletions_size());
  batches.push(batch);
  writeef((uint64_t *)&batch_lock, 1);
  LOG_D_A("%p %ld insertions %ld deletions: Lock released", batch, (long) batch->insertions_size(), (long) batch->deletions_size());
}

/**
* @brief Blocking call to atomically dequeue a received batch.
*
* Blocks until the queue is not empty.
*
* @return A pointer to the batch (YOU MUST HANDLE DELETION)
*/
StingerBatch *
StingerServerState::dequeue_batch()
{
  StingerBatch * rtn = NULL;

  LOG_D("Attempt to acquire lock")
  while(1) {
    readfe((uint64_t *)&batch_lock);
    if(!batches.empty()) {
      rtn = batches.front();
      batches.pop();
      writeef((uint64_t *)&batch_lock, 1);
      break;
    } else {
      writeef((uint64_t *)&batch_lock, 1);
      usleep(100);
    }
  }

  return rtn;
}

size_t
StingerServerState::get_num_streams()
{
  size_t num = 0;
  readfe((uint64_t *)&stream_lock);
  num = streams.size();
  writeef((uint64_t *)&stream_lock, 1);
  return num;
}

StingerStreamState *
StingerServerState::get_stream(size_t index)
{
  StingerStreamState * rtn = NULL;
  readfe((uint64_t *)&stream_lock);
  rtn = streams[index];
  writeef((uint64_t *)&stream_lock, 1);
  return rtn;
}

StingerStreamState *
StingerServerState::get_stream(std::string & name)
{
  StingerStreamState * rtn = NULL;
  readfe((uint64_t *)&stream_lock);
  rtn = stream_map[name];
  writeef((uint64_t *)&stream_lock, 1);
  return rtn;
}

size_t
StingerServerState::get_num_levels()
{
  size_t num = 0;
  readfe((uint64_t *)&alg_lock);
  num = alg_tree.size();
  writeef((uint64_t *)&alg_lock, 1);
  return num;
}

size_t
StingerServerState::get_num_algs()
{
  size_t num = 0;
  readfe((uint64_t *)&alg_lock);
  num = algs.size();
  writeef((uint64_t *)&alg_lock, 1);
  return num;
}

size_t
StingerServerState::get_num_algs(size_t level)
{
  size_t num = 0;
  readfe((uint64_t *)&alg_lock);
  num = alg_tree[level].size();
  writeef((uint64_t *)&alg_lock, 1);
  return num;
}

StingerAlgState *
StingerServerState::get_alg(size_t num)
{
  StingerAlgState * rtn = NULL;
  readfe((uint64_t *)&alg_lock);
  rtn = algs[num];
  writeef((uint64_t *)&alg_lock, 1);
  return rtn;
}

StingerAlgState *
StingerServerState::get_alg(size_t level, size_t index)
{
  StingerAlgState * rtn = NULL;
  readfe((uint64_t *)&alg_lock);
  rtn = alg_tree[level][index];
  writeef((uint64_t *)&alg_lock, 1);
  return rtn;
}

StingerAlgState *
StingerServerState::get_alg(const std::string & name)
{
  StingerAlgState * rtn = NULL;
  readfe((uint64_t *)&alg_lock);
  rtn = alg_map[name];
  writeef((uint64_t *)&alg_lock, 1);
  return rtn;
}

size_t
StingerServerState::add_alg(size_t level, StingerAlgState * alg)
{
  size_t rtn = 0;
  readfe((uint64_t *)&alg_lock);
  alg_tree.resize(level+1);
  alg_tree[level].push_back(alg);
  alg_map[alg->name] = alg;
  rtn = algs.size();
  algs.push_back(alg);
  writeef((uint64_t *)&alg_lock, 1);

  readfe((uint64_t *)&mon_lock);
  server_to_mon.add_dep_name(alg->name);
  server_to_mon.add_dep_data_loc(alg->data_loc);
  server_to_mon.add_dep_description(alg->data_description);
  server_to_mon.add_dep_data_per_vertex(alg->data_per_vertex);
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

bool
StingerServerState::has_alg(const std::string & name)
{
  bool rtn = false;
  readfe((uint64_t *)&alg_lock);
  rtn = alg_map.count(name) > 0;
  writeef((uint64_t *)&alg_lock, 1);
  return rtn;
}

size_t
StingerServerState::get_num_mons()
{
  size_t num = 0;
  readfe((uint64_t *)&mon_lock);
  num = monitors.size();
  writeef((uint64_t *)&mon_lock, 1);
  return num;
}

StingerMonState *
StingerServerState::get_mon(size_t num)
{
  StingerMonState * rtn = NULL;
  readfe((uint64_t *)&mon_lock);
  rtn = monitors[num];
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

StingerMonState *
StingerServerState::get_mon(const std::string & name)
{
  StingerMonState * rtn = NULL;
  readfe((uint64_t *)&mon_lock);
  rtn = monitor_map[name];
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

size_t
StingerServerState::add_mon(StingerMonState * mon)
{
  size_t rtn = 0;
  readfe((uint64_t *)&mon_lock);
  monitor_map[mon->name] = mon;
  rtn = monitors.size();
  monitors.push_back(mon);
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

bool
StingerServerState::has_mon(const std::string & name)
{
  bool rtn = false;
  readfe((uint64_t *)&mon_lock);
  rtn = monitor_map.count(name) > 0;
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

void
StingerServerState::set_mon_stinger(std::string loc, int64_t size) {
  readfe((uint64_t *)&mon_lock);
  server_to_mon.set_stinger_loc(loc);
  server_to_mon.set_stinger_size(size);
  writeef((uint64_t *)&mon_lock, 1);
}


ServerToMon *
StingerServerState::get_server_to_mon_copy() {
  readfe((uint64_t *)&mon_lock);
  ServerToMon * rtn = new ServerToMon(server_to_mon);
  writeef((uint64_t *)&mon_lock, 1);
  return rtn;
}

pthread_t &
StingerServerState::push_thread(pthread_t & thread)
{
  threads.push_back(thread);
  return thread;
}

void
StingerServerState::set_main_loop_thread(pthread_t thread)
{
  main_loop = thread;
}

void
StingerServerState::set_stinger(stinger_t * S)
{
  stinger = S;
}

stinger_t *
StingerServerState::get_stinger()
{
  return stinger;
}

const std::string &
StingerServerState::get_stinger_loc()
{
  return stinger_loc;
}

void
StingerServerState::set_stinger_loc(const std::string & loc)
{
  stinger_loc = loc;
}
