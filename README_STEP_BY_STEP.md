# Hướng dẫn tự xây dựng Mini RTOS từng bước

Tài liệu này dẫn bạn viết lại project từ con số 0. Mỗi bước có bốn phần:

- **Cần làm:** những việc cần phải hoàn thành ở từng bước.
- **Kết quả:** thứ chạy được hoặc kiểm chứng được sau bước đó.
- **Kiến thức dùng:** những gì cần biết để làm bước đó.
- **Học được:** điều đọng lại, và câu hỏi phỏng vấn liên quan.

Thứ tự được chọn để **sau mỗi bước đều build và test được**. Đừng viết hết rồi mới build.
Gõ lệnh trong thư mục `freertos_mock`.

---

## Bước 0 — Chuẩn bị môi trường

**Cần làm**
- Cài w64devkit (gcc + make) vào `C:\w64devkit`, thêm `C:\w64devkit\bin` vào PATH, mở lại terminal.
- Tạo cây thư mục:
  ```
  mkdir include src\core src\port\host src\port\sim examples\demo tests docs
  ```

**Kết quả:** `gcc --version` và `make --version` in ra phiên bản.

**Kiến thức dùng:** biến môi trường PATH, quy trình biên dịch C (preprocess → compile → assemble → link).

**Học được:** vì sao tách `include/` (API công khai) khỏi `src/` (cài đặt), và `build/` (sản phẩm sinh ra, không commit).

---

## Bước 1 — Makefile và test framework tối giản

**Cần làm**
- [Makefile](Makefile): các target `all`, `lib`, `demo`, `run`, `test`, `clean`.
- [tests/test_framework.h/.c](tests/test_framework.h): macro `TEST_ASSERT`, `TEST_ASSERT_EQ`, `RUN_TEST`, hàm `test_framework_report()`.
- [tests/test_main.c](tests/test_main.c): hàm `main` gọi từng suite và trả về mã lỗi.

**Kết quả:** `make test` chạy được, in `0 tests, 0 checks, 0 failed`.

**Kiến thức dùng**
- Makefile: biến, `wildcard`, `patsubst`, pattern rule `%.o: %.c`, `-MMD -MP` để tự sinh phụ thuộc header, biến theo target (`$(CORE_OBJS): INCLUDES += ...`).
- Cờ cảnh báo: `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Macro nhiều dòng `do { ... } while (0)`, `#expr` (stringify), `__FILE__`, `__LINE__`.

**Học được:** viết framework test chỉ cần vài chục dòng, và có `-Werror` ngay từ đầu rẻ hơn nhiều so với sửa cả đống cảnh báo về sau.
*Phỏng vấn:* "Vì sao macro dùng `do { } while (0)`?" (để dùng an toàn trong `if/else` có dấu `;`).

---

## Bước 2 — Kiểu dữ liệu, mã lỗi, cấu hình

**Cần làm**
- [include/os_types.h](include/os_types.h): `os_tick_t`, `os_priority_t`, enum `os_status_t`, hàm `os_status_to_string()` (cài trong [os_status.c](src/core/os_status.c)).
- [include/os_config.h](include/os_config.h): `OS_CFG_MAX_TASKS`, `OS_CFG_MAX_QUEUES`, `OS_ASSERT`... mỗi macro bọc trong `#ifndef` để ghi đè từ dòng lệnh.

**Kết quả:** build được thư viện rỗng. `make DEFINES=-DOS_CFG_MAX_TASKS=32` đổi được cấu hình mà không sửa code.

**Kiến thức dùng:** `stdint.h` (kiểu có độ rộng cố định), enum, include guard, `#ifndef` làm giá trị mặc định, `extern "C"` cho C++.

**Học được:** mọi hàm có thể thất bại đều trả mã lỗi thay vì trả `void`. Cấu hình lúc biên dịch là cách thường dùng trong embedded.

---

## Bước 3 — Intrusive linked list

**Cần làm**
- [src/core/os_list.h/.c](src/core/os_list.h): danh sách liên kết đôi vòng có node sentinel, node nằm trong struct chủ. Gồm `push_back`, `insert_sorted` (ổn định), `remove` O(1), `pop_front`, macro `OS_CONTAINER_OF`.
- [tests/test_list.c](tests/test_list.c): thêm `RUN_TEST` vào suite mới.

**Kết quả:** 4 test pass: list rỗng, FIFO, xóa giữa danh sách, chèn có thứ tự và ổn định.

**Kiến thức dùng:** con trỏ và con trỏ-tới-con trỏ, `offsetof`, ép kiểu qua `char *` để tính địa chỉ struct chủ, con trỏ hàm làm tiêu chí so sánh (`os_list_less_fn_t`).

**Học được:** vì sao node sentinel loại bỏ mọi `if` cho trường hợp danh sách rỗng, và vì sao cách "node nằm trong object" không cần cấp phát. Đây là cùng kỹ thuật với `list_head` của Linux.

---

## Bước 4 — Port layer và so sánh tick an toàn

**Cần làm**
- [include/os_port.h](include/os_port.h): giao diện với phần cứng, gồm `os_port_get_tick`, `os_port_idle`, `os_port_enter_critical`, `os_port_exit_critical`.
- [src/port/sim/](src/port/sim/os_port_sim.c): đồng hồ ảo, chỉ là một biến `s_tick`, có hàm `set_tick`, `advance`.
- [src/port/host/os_port_host.c](src/port/host/os_port_host.c): đồng hồ thật (`QueryPerformanceCounter` trên Windows, `clock_gettime` trên POSIX).
- [src/core/os_tick.h](src/core/os_tick.h): `os_tick_reached(now, deadline)`.

**Kết quả:** kernel đã biết "bây giờ là mấy giờ" mà không phụ thuộc hệ điều hành cụ thể. Test dùng đồng hồ ảo nên chạy tức thì.

**Kiến thức dùng:** biên dịch có điều kiện (`#if defined(_WIN32)`), API thời gian của OS, số học không dấu 32-bit, ép kiểu `int32_t` để so sánh vòng.

**Học được:**
- Tách phần phụ thuộc phần cứng ra một lớp nhỏ thì core test được trên PC và port được lên MCU.
- Tick 32-bit tràn sau 49.7 ngày. Dùng `(int32_t)(now - deadline) >= 0` thay vì `now >= deadline`.


---

## Bước 5 — Block pool: free list in-place

**Cần làm**
- [src/core/os_block_pool.h/.c](src/core/os_block_pool.c): chia vùng nhớ thành block đều nhau, block rảnh lưu con trỏ `next` ngay trong nó. Có `init`, `alloc` O(1), `free` O(1), `owns` (kiểm tra con trỏ hợp lệ), phát hiện double-free bằng marker.

**Kết quả:** một bộ cấp phát không dùng `malloc`, không bị phân mảnh. Các bước sau tái sử dụng nó cho TCB, queue và mempool.

**Kiến thức dùng:** `memcpy` để đọc/ghi node (tránh vi phạm strict aliasing), `uintptr_t` để tính toán địa chỉ, `alignof`, `_Static_assert`, kiểm tra biên và căn chỉnh của con trỏ.

**Học được:** cách chặn con trỏ rác ở O(1) (nằm trong vùng pool *và* đúng biên block), và bài toán double-free: kiểm tra nhanh trước, chỉ tốn O(n) khi có nghi ngờ.

*Phỏng vấn:* "Memory pool khác heap thế nào? Vì sao embedded thích dùng?"

---

## Bước 6 — Task và opaque pointer

**Cần làm**
- [include/os_task.h](include/os_task.h): `typedef struct os_task *os_task_handle_t;` (chỉ khai báo, không định nghĩa), `os_task_config_t`, các hàm create/delete/suspend/resume/notify/get_info.
- [src/core/os_task_internal.h](src/core/os_task_internal.h): định nghĩa thật của TCB, chỉ file trong `src/core` được include.
- [src/core/os_task.c](src/core/os_task.c): pool TCB, kiểm tra handle hợp lệ, máy trạng thái task.
- [tests/test_task.c](tests/test_task.c).

**Kết quả:** tạo và xóa task được, handle sai/cũ/ngoại lai bị từ chối, pool cạn trả `OS_ERR_NO_MEMORY`.

**Kiến thức dùng:** kiểu chưa hoàn chỉnh (incomplete type), con trỏ hàm `void (*)(void *)` làm entry của task, `void *arg` để truyền ngữ cảnh, struct khởi tạo bằng designated initializer.

**Học được:** opaque pointer là cách "private" trong C. Hàm API nhận `NULL` để chỉ "task đang chạy" (giống FreeRTOS). Task tự xóa chính mình phải hoãn đến khi callback return.
*Phỏng vấn:* "Vì sao không để người dùng thấy struct?"

---

## Bước 7 — Scheduler (trái tim của project)

**Cần làm**
- [src/core/os_sched_internal.h](src/core/os_sched_internal.h): giao diện giữa scheduler và task.
- [src/core/os_scheduler.c](src/core/os_scheduler.c) gồm:
  - hai danh sách: *ready* (sắp theo ưu tiên) và *delayed* (sắp theo thời điểm release);
  - `os_scheduler_run_once()`: đưa task đến hạn sang ready, lấy task ưu tiên cao nhất, chạy tới khi xong;
  - `os_scheduler_start()`: vòng lặp vô hạn, rảnh thì gọi `os_port_idle`;
  - `sched_advance_release()`: tính mốc kế tiếp không trôi và đếm overrun.
- [include/os_scheduler.h](include/os_scheduler.h).
- [tests/test_helpers.h/.c](tests/test_helpers.h) và [tests/test_scheduler.c](tests/test_scheduler.c).

**Kết quả:** 15 test: chu kỳ đúng, ưu tiên đúng, FIFO khi bằng nhau, suspend/resume/delete, tick wraparound, overrun, `start` kết thúc sau `stop`.

**Kiến thức dùng:** super-loop, phân biệt cooperative và preemptive, hai danh sách của scheduler, `volatile` cho cờ có thể bị đổi từ ISR/signal, critical section, máy trạng thái (READY / RUNNING / WAITING / SUSPENDED).

**Học được:**
- `next_release += period` (không dùng `now + period`) để chu kỳ không bị trôi.
- Vì sao chạy bù mọi lần trễ sẽ làm đói task thấp, nên chọn bỏ qua và đếm overrun.
- Cách test code phụ thuộc thời gian bằng đồng hồ ảo.


---

## Bước 8 — Message queue

**Cần làm**
- [include/os_queue.h](include/os_queue.h) và [src/core/os_queue.c](src/core/os_queue.c): ring buffer lưu item theo giá trị, storage do người dùng cấp. Có `send`, `receive`, `peek`, `count`, `space`, `set_receiver` (gửi xong thì đánh thức task nhận).
- [src/core/os_queue_internal.h](src/core/os_queue_internal.h): `os_queue_on_task_deleted()` để gỡ liên kết khi task bị xóa.
- [tests/test_queue.c](tests/test_queue.c).

**Kết quả:** mô hình producer/consumer chạy được. Task nhận là task theo sự kiện, chỉ chạy khi queue có dữ liệu.

**Kiến thức dùng:** ring buffer (chỉ số `head` và `count`, lấy `% capacity`), `memcpy` với `item_size` runtime (generic bằng `void *`), kiểm tra tràn số `capacity > SIZE_MAX / item_size`, thông báo ra ngoài critical section để tránh gọi lồng nhau.

**Học được:** vì sao queue kiểu copy an toàn hơn kiểu con trỏ, vì sao thông báo nhiều lần được gộp lại thành một lần kích hoạt, và vì sao phải gỡ `receiver` khi task bị xóa (slot được tái sử dụng sẽ bị đánh thức nhầm).

---

## Bước 9 — Memory pool công khai và zero-copy

**Cần làm**
- [include/os_mempool.h](include/os_mempool.h) và [src/core/os_mempool.c](src/core/os_mempool.c): bọc block pool thành API cho ứng dụng. Số block tính từ kích thước storage, block làm tròn theo `max_align_t`, có `get_info` với watermark, macro `OS_MEMPOOL_DECLARE_STORAGE`.
- Chuyển TCB pool và queue pool sang dùng `os_block_pool` (refactor: các test cũ phải vẫn pass).
- [tests/test_mempool.c](tests/test_mempool.c).

**Kết quả:** 12 test mới, trong đó có truyền một frame qua queue chỉ bằng con trỏ.

**Kiến thức dùng:** `alignof`/`max_align_t`, macro tính kích thước làm tròn lên bội số, refactor có lưới an toàn là test.

**Học được:** zero-copy và quy tắc *ownership* (gửi xong không được chạm, nhận xong phải free), watermark để cỡ pool, và vì sao refactor an toàn nhờ test đã có.

*Phỏng vấn:* "Double free phát hiện thế nào?"

---

## Bước 10 — Ứng dụng demo nhiều lớp

**Cần làm** (thư mục [examples/demo/](examples/demo/))
- Lớp phần cứng giả lập: `bsp_led`, `bsp_temp_sensor`.
- Lớp tiện ích: `app_log` (log kèm timestamp), `app_messages.h` (định nghĩa frame).
- Lớp ứng dụng: `app_blink`, `app_sensor` (producer), `app_logger` (consumer), `app_supervisor` (kịch bản suspend/resume/delete/stop).
- [main.c](examples/demo/main.c): khởi tạo, tạo task, chạy scheduler, in thống kê.

**Kết quả:** `make run` chạy 8 giây: LED nháy, sensor gửi frame, logger xử lý, supervisor cố tình suspend logger để pool cạn và sensor phải drop frame.

**Kiến thức dùng:** kiến trúc phân lớp (bsp → app → main), mỗi task một cặp `.h/.c`, struct ngữ cảnh truyền qua `void *arg` thay cho biến toàn cục, `printf` với `PRIu32`, `__attribute__((format(printf)))`.

**Học được:** viết ứng dụng thật trên API của mình để phát hiện API khó dùng ở đâu. Demo cũng là bằng chứng cho người phỏng vấn thấy hệ thống hoạt động.

---

## Bước 11 — Kiểm tra chất lượng và đóng gói

**Cần làm**
- Build thêm cấu hình release: `make BUILD=release all test`. Cấu hình này thường lộ cảnh báo mà debug không có.
- `make SANITIZE=1 test` (ASan + UBSan, chạy trên Linux/macOS hoặc WSL).
- `make check` (cppcheck) và `make format` ([.clang-format](.clang-format)) nếu đã cài các công cụ này.
- Viết [README.md](README.md) và [docs/interview_notes.md](docs/interview_notes.md); thêm [.gitignore](.gitignore).
- `git init`, commit từng bước một thay vì một commit khổng lồ.

**Kết quả:** project sẵn sàng đưa lên GitHub: build sạch cả hai cấu hình, test đều pass, có tài liệu.

**Kiến thức dùng:** cờ tối ưu `-O0` và `-O2 -DNDEBUG`, sanitizer, phân tích tĩnh, Git.

**Học được:** các lỗi về undefined behavior thường chỉ xuất hiện ở `-O2`. Sanitizer bắt lỗi bộ nhớ mà test thường không thấy. Tài liệu và commit gọn là thứ nhà tuyển dụng đọc đầu tiên.

---

## Bảng tổng hợp

| Bước | Sản phẩm | Kiến thức C chính |
|---|---|---|
| 0–1 | Môi trường, Makefile, test framework | Make, macro, cờ cảnh báo |
| 2 | Kiểu, mã lỗi, cấu hình | `stdint.h`, enum, `#ifndef` |
| 3 | Intrusive list | Con trỏ, `offsetof`, con trỏ hàm |
| 4 | Port layer, tick an toàn | `#if`, số học không dấu |
| 5 | Block pool | `uintptr_t`, alignment, aliasing |
| 6 | Task, opaque handle | Incomplete type, đóng gói |
| 7 | Scheduler | Super-loop, máy trạng thái, `volatile` |
| 8 | Message queue | Ring buffer, `void *` generic |
| 9 | Memory pool, zero-copy | Alignment, ownership, refactor |
| 10 | Demo nhiều lớp | Kiến trúc phân lớp |
| 11 | Chất lượng, đóng gói | Sanitizer, release build, Git |

## Gợi ý khi tự làm lại

0. Cần phải tìm hiểu qua trước các thuật ngữ cơ cản của RTOS như scheduler, task, memory pool là gì?
1. **Tự viết trước, so với mã mẫu sau.** Mỗi bước đọc phần "Cần làm" rồi tự viết file, sau đó mới mở file mẫu so sánh.
2. **Viết test trước hoặc cùng lúc** với mã của bước đó, không để dồn lại.
3. **Mỗi bước một commit**, ví dụ `step 3: intrusive list + tests`.
4. **Sau mỗi bước tự trả lời** các câu "Phỏng vấn" bằng lời của mình, không nhìn tài liệu.
5. Đã hiểu hết thì làm các hướng mở rộng: bitmap ưu tiên O(1), software timer, tickless idle, port lên ESP32.
