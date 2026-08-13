# C++ 声明修饰符 / 属性速查

> 用途：`[[...]]` 属性和 `override`/`final`/`noexcept` 这类"同一位置"的修饰符，各自该放哪、怎么组合。
> 核心心智模型：**每个修饰符有固定的语法槽位**。先记位置图，再记每格能放什么。

---

## 0. 位置图（成员函数声明的槽位）

```
[属性] [virtual/static/explicit/inline/constexpr] 返回类型 函数名(参数)
        [const] [&或&&] [noexcept] [属性] [override|final] [=default|=delete] [-> 尾置返回]
        { 函数体 }
```

一个函数上能同时堆多少（每格一个）：

```cpp
[[nodiscard]] virtual bool isValid() const noexcept override = 0;
// └─属性─┘└─virtual─┘         └const┘└noexcept┘ └override┘ └纯虚┘
```

| 槽位 | 修饰符 | 一句话 |
|---|---|---|
| 属性 `[[...]]` | `nodiscard`/`maybe_unused`/`deprecated`/`noreturn`/`likely`… | 编译器提示/警告，不影响类型 |
| 声明开头 | `virtual`/`static`/`explicit`/`inline`/`constexpr`/`friend` | 函数的类别属性 |
| 签名后·体前 | `const`/`noexcept`/`&`/`&&` | 限定 this 的性质 |
| 签名最后 | `override`/`final` | 覆盖关系 |
| 代替函数体 | `= default`/`= delete`/`= 0` | 让编译器生成 / 禁止 / 抽象 |
| 尾部 | `-> 类型`（配合 `auto`） | 尾置返回类型 |

---

## 1. 属性 `[[...]]`

> 编译器"不认识就直接忽略"（只要语法合法）。所以属性是**可移植的建议**，不是强约束。
> vendor 专属如 `[[gnu::always_inline]]` 只有对应编译器认。

| 属性 | 版本 | 作用 | 例子 |
|---|---|---|---|
| `[[nodiscard]]` | C++17 | 返回值被忽略时警告 | `[[nodiscard]] bool init();` |
| `[[nodiscard("msg")]]` | C++20 | 带自定义警告文本 | `[[nodiscard("必须处理错误")]] Err make();` |
| `[[maybe_unused]]` | C++17 | 抑制"未使用"警告 | `[[maybe_unused]] int debugCnt;` |
| `[[deprecated]]` | C++14 | 使用时报弃用警告 | `[[deprecated("用 create 替代")]] void old();` |
| `[[noreturn]]` | C++11 | 函数永不返回 | `[[noreturn]] void fatal() { std::abort(); }` |
| `[[fallthrough]]` | C++17 | case 穿透是有意的 | `case 1: doA(); [[fallthrough]];` |
| `[[likely]]`/`[[unlikely]]` | C++20 | 分支概率提示 | `if ([[likely]] fast) {...}` |
| `[[no_unique_address]]` | C++20 | 空成员占 0 字节（EBO） | `[[no_unique_address]] EmptyTag tag;` |

`[[nodiscard]]` 特别说明：
- 可以标在**函数**、**类**、**枚举**上。标在类/枚举上 → 任何返回它的函数都必须处理返回值。
- C++20 起可标在**构造函数**上（禁止忽略显式转换产生的临时对象）。

---

## 2. 声明开头的类修饰符

| 修饰符 | 版本 | 作用 | 例子 / 陷阱 |
|---|---|---|---|
| `virtual` | C++98 | 虚函数，可被派生类覆盖 | 只能写在**类内声明**处，类外定义处不能再写 `virtual` |
| `static` | C++98 | 成员函数无 `this`；静态数据成员 | `static` 成员函数**不能** `virtual`、**不能** `const` |
| `explicit` | C++11 | 禁止隐式转换 | `explicit` 构造函数 + `explicit operator bool()` |
| `inline` | C++17(变量) | 允许多 TU 重复定义并合并 | 头文件里定义函数/变量要加（`constexpr` 成员隐式 inline） |
| `constexpr` | C++11 | 可编译期求值（不强求） | C++14 放宽到允许循环/分支 |
| `consteval` | C++20 | 强制编译期求值 | 运行期调用直接编译错 |
| `constinit` | C++20 | 静态初始化期完成初始化，之后可变 | `constinit` ≠ `const`，只是保证初始化时机 |
| `friend` | C++98 | 授予私有成员访问 | 声明在类内，定义在类外 |

`explicit` 的两个经典用法：

```cpp
class Handle {
    explicit Handle(uint64_t id);          // 不能从 uint64_t 隐式构造
    explicit operator bool() const;        // if(h) 可以；int x = h; 不行
};
```

---

## 3. 签名之后、体之前（与 `override` 同槽位）

按语法顺序：`const` → `&/&&` → `noexcept` → `override`/`final`

| 修饰符 | 版本 | 作用 | 例子 |
|---|---|---|---|
| `const` | C++98 | 常成员函数（`this` 是 const） | `void draw() const;` |
| `&` / `&&` | C++11 | ref-qualifier，按 this 的值类别重载 | `void push(T&&) &&;` |
| `noexcept` | C++11 | 保证不抛；抛了 → `std::terminate` | `void swap(T& o) noexcept;` |
| `noexcept(expr)` | C++11 | 条件式 noexcept | `noexcept(noexcept(std::swap(a,b)))` |
| `override` | C++11 | 明确覆盖基类虚函数，**签名不匹配立刻编译错** | `bool isIdle() const override;` |
| `final` | C++11 | 函数不可再被覆盖；标在类上 = 不可被继承 | `void step() final;` |

`override` 为什么是 C++11 最重要的补丁：漏写 `override` 时，若签名写错，新函数只是**隐藏**了基类虚函数，编译不报错、运行调错版本。写了 `override` 则编译器帮你校验。

`noexcept` 特别说明：
- C++17 起 `noexcept` 是**函数类型的一部分**，`void f() noexcept` 和 `void f()` 是不同类型（指向函数的指针也有别）。
- 析构函数**默认** `noexcept`。
- 覆盖基类虚函数时，派生类可以比基类更严格（加 `noexcept`），不能更宽松。

`&`/`&&`（ref-qualifier）示例：

```cpp
struct Buffer {
    const Buffer& data() const &  { return *this; }  // 左值对象调用
    Buffer        data() &&       { return std::move(*this); }  // 右值对象调用
};
```

---

## 4. 代替函数体的三件套

| 写法 | 含义 | 例子 |
|---|---|---|
| `= default` | 显式要求编译器生成该特殊成员函数 | `virtual ~FrameContext() = default;` |
| `= delete` | 删除该函数，任何使用都是编译错 | `FrameContext(const FrameContext&) = delete;` |
| `= 0` | 纯虚 → 类是抽象类，不可实例化 | `virtual void render() = 0;` |

常见组合：
- 禁止拷贝 → `= delete` 拷贝构造 + 拷贝赋值（你引擎里 SwapChain/FrameContext/Instance 都这么写）
- 多态基类析构 → `virtual ~T() = default;`
- `= delete` 还能删**非成员**函数，用来挡掉不需要的隐式转换重载：
  ```cpp
  void open(const char* path);
  void open(std::string_view path);
  void open(int) = delete;  // open(42) 编译错，而不是悄悄转成别的
  ```

`= 0` 和 `= delete` 都在声明最后、长得像，别混：`= 0` 是"抽象"（要派生类实现），`= delete` 是"禁止"（谁用谁错）。

---

## 5. 尾置返回类型 `->`

```cpp
auto  f() -> int;          // 等价于 int f()
auto  f() -> decltype(a+b) // 返回类型依赖表达式/参数 → 尾置才能写
decltype(auto) f() { return x; }  // 保持 x 的精确类型（含引用/const）
```

主用途：模板里返回类型依赖参数时，声明时还写不出返回类型，只能尾置。

---

## 6. 常见陷阱汇总

1. **`override` 漏写**：不报错，但函数变成"隐藏"而非"覆盖"基类虚函数。
2. **`virtual` 写错位置**：只能类内声明处；类外定义处不能写。
3. **`= default`/`= delete` 与函数体互斥**：写了体就不能 default/delete。
4. **`explicit` 只在声明处**：定义处不能再写。
5. **`static` 成员函数不能是 `virtual`/`const`**。
6. **析构默认 `noexcept`**：想让它抛要 `noexcept(false)`，几乎从不需要。
7. **`noexcept` 在 C++17 进函数类型**：函数指针赋值、虚覆盖都要对得上。
8. **属性不认识就忽略**：所以 `[[nodiscard]]` 只是建议；要硬约束靠 `static_assert` / 测试。
9. **`static_assert` 不是修饰符是声明**，但常和 `constexpr` 一起做编译期检查（你的 RHI 句柄位布局就这么干的）。

---

## 7. 你的引擎里已经在用的

| 用法 | 位置 |
|---|---|
| `= delete` 禁拷贝 | `Swapchain.hpp:36` `FrameContext.hpp:76` `Instance.hpp:53` |
| `noexcept` | `Instance.hpp:55-56`（移动构造/赋值）、`Types.hpp:68`（operator()） |
| `explicit` + `constexpr` + `noexcept` 组合 | `RHI_HANDLES_SYSTEM.hpp:24-31`（`explicit operator bool() const noexcept`） |
| `static_assert` 编译期检查 | `RHI_HANDLES_SYSTEM.hpp:20`（句柄位布局） |
| `= default` | `RHI_HANDLES_SYSTEM.hpp:23`（`constexpr ResourceHandle() = default;`） |
