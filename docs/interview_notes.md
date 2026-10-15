# Ghi chú phỏng vấn — Mini RTOS

## 1. Giới thiệu trong 30 giây

> "Em đã viết một kernel RTOS cooperative bằng C11, API lấy cảm hứng từ FreeRTOS. Task là các
> callback chạy tới khi xong (run-to-completion), được lập lịch theo độ ưu tiên, có thể chạy định
> kỳ hoặc theo sự kiện, và giao tiếp với nhau qua message queue. Kernel không dùng malloc: mọi object
> đều cấp phát từ pool tĩnh qua free list in-place với độ phức tạp O(1). Có thêm memory pool để truyền
> dữ liệu zero-copy, giống lwIP. Cấu trúc bên trong được giấu sau opaque handle. Phần core tách khỏi phần
> cứng qua một port layer, nhờ đó unit test chạy trên đồng hồ ảo, nhanh và lần nào cũng cho cùng
> kết quả."

## 2. Đi qua code theo thứ tự này

1. [include/os_task.h](../include/os_task.h): `typedef struct os_task *os_task_handle_t;` là con trỏ tới kiểu chưa hoàn chỉnh (opaque pointer).
2. [src/core/os_task_internal.h](../src/core/os_task_internal.h): định nghĩa thật của TCB, chỉ kernel mới thấy.
3. [src/core/os_scheduler.c](../src/core/os_scheduler.c): các hàm `os_scheduler_run_once()`, `sched_dispatch()`, `sched_advance_release()`.
4. [src/core/os_list.c](../src/core/os_list.c): intrusive list và `OS_CONTAINER_OF`.
5. [src/core/os_block_pool.c](../src/core/os_block_pool.c): free list in-place, phát hiện double-free bằng marker.
6. [examples/demo/app_sensor.c](../examples/demo/app_sensor.c) → [app_logger.c](../examples/demo/app_logger.c): zero-copy và chuyển ownership.
7. [tests/test_scheduler.c](../tests/test_scheduler.c): test tick wraparound và test overrun.

## 3. Kiến thức C thể hiện trong project

| Kiến thức | Vị trí trong code |
|---|---|
| Function pointer | `os_task_fn_t` (task), `os_list_less_fn_t` (hàm so sánh truyền vào list) |
| Opaque pointer / đóng gói | `os_task_handle_t`, `os_queue_handle_t`; struct chỉ định nghĩa trong file `.c` hoặc header private |
| Struct và cấu trúc dữ liệu | TCB, intrusive doubly-linked list có sentinel, ring buffer |
| `offsetof` / container_of | `OS_CONTAINER_OF` trong `os_list.h` |
| `static` cho internal linkage | các pool và mọi hàm nội bộ |
| `volatile` | `s_running` (có thể bị clear từ signal handler hoặc ISR) |
| Số học không dấu và tràn số | `(int32_t)(now - deadline) >= 0` |
| `memcpy` để copy dữ liệu generic | queue lưu item bất kỳ theo `item_size` |
| Kiểm tra tràn số | `capacity > SIZE_MAX / item_size`, `block_size > SIZE_MAX - OS_MEMPOOL_ALIGN` |
| Alignment | `alignof(max_align_t)`, `OS_MEMPOOL_DECLARE_STORAGE` khai báo mảng `max_align_t` |
| Strict aliasing | `block_read_node()` / `block_write_node()` dùng `memcpy` thay vì ép kiểu con trỏ |
| `_Static_assert` | kiểm tra lúc biên dịch rằng node của free list vừa trong block nhỏ nhất |
| Conditional compilation | `os_config.h` (`#ifndef` để override cấu hình), port host `#if defined(_WIN32)` |

## 4. Câu hỏi thường gặp và cách trả lời

**Cooperative khác preemptive thế nào?**
Cooperative: task chạy tới khi tự return, không bị ngắt giữa chừng. Vì vậy không cần stack riêng
cho từng task, không cần context switch bằng assembly, và không cần mutex giữa các task. Đổi lại,
một task chạy lâu sẽ làm trễ mọi task khác, kể cả task ưu tiên cao. Preemptive (như FreeRTOS) có
thể ngắt task đang chạy (trên Cortex-M dùng SysTick + PendSV), mỗi task có stack riêng, và phải
xử lý race condition bằng mutex hoặc semaphore.

**Vì sao không dùng malloc?**
Trong embedded: heap bị phân mảnh sau thời gian dài chạy, thời gian cấp phát không xác định, và
có thể hết bộ nhớ lúc runtime. Pool tĩnh cho biết chính xác dung lượng RAM ngay lúc link và không
bao giờ thất bại sau khi khởi tạo. MISRA C cũng cấm cấp phát động.

**Opaque pointer mang lại lợi ích gì?**
Người dùng không đọc hay sửa được field bên trong, nên không thể làm hỏng trạng thái kernel. Có
thể đổi layout của struct mà không phải sửa code ứng dụng. Header public gọn và ổn định. Đánh
đổi là người dùng không cấp phát struct trên stack của họ được, nên kernel phải tự cấp phát (ở
project này là pool tĩnh).

**Tại sao dùng intrusive list?**
Node nằm ngay trong TCB nên chuyển task giữa ready list và delayed list không cần cấp phát gì.
Xóa node là O(1) vì danh sách liên kết đôi. Linux (`list_head`) và FreeRTOS (`ListItem_t`) đều
dùng cách này.

**Độ phức tạp của scheduler?**
Chèn vào ready list là O(n) vì phải giữ thứ tự sắp xếp, còn lấy task kế tiếp là O(1). FreeRTOS
dùng mảng list theo từng mức ưu tiên cộng bitmap: chèn O(1), tìm mức ưu tiên cao nhất bằng lệnh
CLZ cũng O(1). Đó là bước cải tiến tự nhiên tiếp theo.

**Tick 32-bit bị tràn sau 49.7 ngày thì sao?**
So sánh `now >= deadline` sẽ sai khi counter quay về 0. Kernel so sánh hiệu có dấu,
`(int32_t)(now - deadline) >= 0`, và cách này đúng miễn là hai mốc thời gian cách nhau không quá
2^31 ms. Có unit test chạy xuyên qua thời điểm `0xFFFFFFFF → 0`.

**Làm sao để chu kỳ không bị trôi (drift)?**
`next_release += period` thay vì `next_release = now + period`. Thời gian thực thi và jitter vì
thế không bị cộng dồn qua các chu kỳ. Demo cho thấy LED vẫn đúng mốc 500/1000/1500 ms.

**Overrun xử lý thế nào?**
Nếu task chạy xong mà đã trễ qua một hoặc nhiều mốc release kế tiếp, các mốc đó bị bỏ qua và được
đếm vào `overrun_count`. Nếu cho task chạy bù dồn dập thì các task ưu tiên thấp sẽ bị đói (starve).

**Task tự xóa chính nó thì sao?**
Callback của nó vẫn đang nằm trên stack, nên kernel chỉ đánh dấu `delete_pending`. Khi callback
return, scheduler mới thu hồi slot. Đồng thời kernel gỡ liên kết task khỏi mọi queue
(`os_queue_on_task_deleted`), để slot được tái sử dụng sau này không bị đánh thức nhầm.

**Critical section để làm gì khi chỉ có một luồng?**
Trên MCU, interrupt có thể gọi `os_queue_send` hoặc `os_task_notify` trong lúc scheduler đang
sửa list. Port MCU sẽ tắt/bật interrupt ở hai hàm này. Port host chỉ đếm độ lồng nhau (nesting)
để phát hiện lỗi enter/exit không cân bằng, và có unit test kiểm tra điều đó.

**Test thế nào khi code phụ thuộc thời gian thực?**
Bằng dependency injection ở mức link: test link với `os_port_sim.c`, trong đó đồng hồ là một biến.
Test tự tua thời gian, có thể tạo tình huống tick wraparound hay task chạy 250 ms, và chạy xong
toàn bộ trong vài mili giây.

**Memory pool hoạt động thế nào? Vì sao alloc/free là O(1)?**
Storage được chia thành các block cùng kích thước. Block rảnh dùng chính vùng nhớ của nó để lưu
con trỏ `next`, nên free list không tốn thêm RAM. Alloc là lấy phần tử đầu (pop head), free là đẩy
vào đầu (push head): cả hai đều O(1), không phải tìm kiếm và không có phân mảnh. Đây là nền tảng
của `osMemoryPool` (CMSIS-RTOS), `memp` của lwIP và slab allocator của Linux.

**So với heap_1..heap_5 của FreeRTOS thì sao?**
heap_x là allocator cho block có kích thước tùy ý. heap_4 và heap_5 dùng first-fit kèm gộp (coalesce)
các block rảnh liền kề, nhưng vẫn có thể bị phân mảnh và thời gian cấp phát phụ thuộc vào độ dài
danh sách block rảnh. Memory pool chỉ phục vụ một kích thước, đổi lại có O(1) và không phân mảnh.
Trên thực tế người ta dùng cả hai: heap cho khởi tạo, pool cho đường dữ liệu tần suất cao.

**Double free được phát hiện thế nào khi không có bitmap?**
Block rảnh mang hai giá trị là `next` và một `marker` riêng cho từng pool. Khi free:
- Không thấy marker: block chắc chắn đang được cấp phát, nên free hợp lệ, O(1).
- Có marker: duyệt free list. Nếu tìm thấy block thì đó là double free (trả về `OS_ERR_INVALID_STATE`).
  Nếu không thấy thì chỉ là dữ liệu người dùng trùng giá trị, và free vẫn hợp lệ.
Lúc alloc, marker bị xóa để lần free sau đi đường nhanh. Có unit test cho cả trường hợp dữ liệu
người dùng giả trùng marker.

**Zero-copy được lợi gì, và rủi ro là gì?**
Lợi ích: payload chỉ được ghi một lần, queue chỉ copy con trỏ (8 byte) dù frame dài 40 byte hay 1500
byte. Rủi ro: hai bên phải thống nhất về ownership. Bên gửi không được chạm vào frame sau khi gửi,
và bên nhận phải free. Nếu quên free thì pool cạn dần (leak); nếu free hai lần thì kernel phát hiện
được. Demo cố tình suspend logger để pool cạn, cho thấy sensor drop frame một cách có kiểm soát
chứ không bị crash, và `min_free` (watermark) cho biết nên cấu hình pool lớn bao nhiêu.

**Vì sao đọc/ghi node bằng `memcpy` thay vì ép kiểu `(node_t *)block`?**
Storage của TCB pool được khai báo là `struct os_task[]`. Truy cập vùng đó qua một kiểu khác vi
phạm quy tắc strict aliasing (undefined behaviour), và compiler ở `-O2` được phép sắp xếp lại các lệnh
đọc/ghi. `memcpy` luôn hợp lệ, và compiler biên dịch nó thành hai lệnh mov nên không tốn chi phí.

## 5. Điểm yếu nên chủ động nói ra (và cách cải thiện)

- Không preemptive, nên latency của task ưu tiên cao phụ thuộc vào task dài nhất.
- Handle cũ trỏ tới slot đã được tái sử dụng thì không phát hiện được. Cách sửa: mã hóa thêm generation counter vào handle.
- Memory pool không phát hiện được use-after-free: ghi vào block sau khi free sẽ làm hỏng free list. Cách sửa trong bản debug: tô (poison) block khi free và kiểm tra lại khi alloc.
- Mỗi pool chỉ phục vụ một kích thước. Muốn dùng nhiều kích thước thì tạo nhiều pool (ví dụ 64 B / 256 B / 1500 B như lwIP) và chọn pool nhỏ nhất vừa với dữ liệu.
- Hướng mở rộng: bitmap priority O(1), software timer one-shot, tickless idle, port lên STM32 (SysTick, `__disable_irq`, `WFI`), CI bằng GitHub Actions chạy `make test SANITIZE=1`.
