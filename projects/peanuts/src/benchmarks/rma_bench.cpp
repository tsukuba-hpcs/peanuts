#include <mpi.h>
#include <stdio.h>
#include <cstddef>
#include <cstring>
#include <vector>

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int my_rank;
  int world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int target_rank = (my_rank + world_size / 2) % world_size;

  constexpr size_t xfer_size = 1 << 12;  // 4 KiB
  // constexpr size_t xfer_size = 47'008;
  // constexpr size_t xfer_size = 1 << 21;  // 2 MiB
  constexpr size_t num_iter = 1000;
  // constexpr size_t num_iter = 20;
  constexpr size_t block_size = xfer_size * num_iter;

  std::vector<std::byte> recv_buf(block_size, std::byte{1});

  void* ptr = nullptr;
  MPI_Alloc_mem(block_size, MPI_INFO_NULL, &ptr);
  // memset(ptr, 1, block_size);

  // void* recv_ptr = nullptr;
  // MPI_Alloc_mem(block_size, MPI_INFO_NULL, &recv_ptr);

  MPI_Win win;
  MPI_Win_create(ptr, block_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Win_lock(MPI_LOCK_SHARED, target_rank, 0, win);
  double res_time = -1.;

  // warn up
  for (size_t i = 0; i < num_iter; ++i) {
    // for (size_t i = 0; i < 20; ++i) {
    // MPI_Get((void*)((char*)recv_ptr + i * xfer_size), xfer_size, MPI_BYTE,
    //         target_rank, i * xfer_size, xfer_size, MPI_BYTE, win);
    MPI_Get(&recv_buf[i * xfer_size], xfer_size, MPI_BYTE, target_rank,
            i * xfer_size, xfer_size, MPI_BYTE, win);
    // MPI_Get(&recv_buf[i * xfer_size], xfer_size, MPI_BYTE, target_rank,
    //         i * xfer_size, xfer_size, MPI_BYTE, win);
    MPI_Win_flush(target_rank, win);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  res_time = MPI_Wtime();
  MPI_Barrier(MPI_COMM_WORLD);

  // if (my_rank == 0) {
  for (size_t i = 0; i < num_iter; ++i) {
    // MPI_Get((void*)((char*)recv_ptr + i * xfer_size), xfer_size, MPI_BYTE,
    //         target_rank, i * xfer_size, xfer_size, MPI_BYTE, win);
    MPI_Get(&recv_buf[i * xfer_size], xfer_size, MPI_BYTE, target_rank,
            i * xfer_size, xfer_size, MPI_BYTE, win);
    MPI_Win_flush(target_rank, win);
  }
  // }
  MPI_Barrier(MPI_COMM_WORLD);
  res_time = MPI_Wtime() - res_time;

  MPI_Win_unlock(target_rank, win);

  if (my_rank == 0) {
    // printf("%lf usec, %lf MB/s\n", res_time * 1000'000 / num_iter,
    //        (double)xfer_size * num_iter / res_time / 1000'000.);
    // aggregate bandwidth
    printf("%lf msec, %lf GB/s\n", res_time * 1000,
           world_size * xfer_size * num_iter / res_time / 1000'000'000);
  }

  MPI_Win_free(&win);

  MPI_Finalize();
  return 0;
}
