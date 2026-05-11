#pragma once
#include "exception.hpp"
#include "algorithm.hpp"
namespace string_container
{
    /*
            * @brief  #### `string` 类

            *   - 自定义字符串容器类，用于存储和操作字符串数据

            *   - 支持字符串的创建、修改、拼接、插入、查找等操作
            *
            *   - 自动管理内存资源，包含异常处理机制

            * 类型别名:

            * * - `iterator`: 字符指针类型，用于遍历字符串
            *
            * * - `const_iterator`: 常量字符指针类型，用于只读遍历
            *
            * * - `reverse_iterator`: 反向迭代器类型（与 iterator 相同）
            *
            * * - `const_reverse_iterator`: 常量反向迭代器类型（与 const_iterator 相同）
            *
            * * - `nops`: 静态常量，值为 -1（可能用于表示无效位置）

            * 成员变量:

            * * - `data_`: 指向字符数组的指针，存储字符串数据
            *
            * * - `size_`: 字符串当前长度（不包含终止符 '\0'）
            *
            * * - `capacity_`: 字符串当前容量（可容纳的最大字符数，含终止符）

            * 迭代器相关方法:

            * * - `begin()`: 返回指向字符串起始位置的迭代器
            *
            * * - `end()`: 返回指向字符串末尾（终止符前）的迭代器
            *
            * * - `cbegin()`: 返回指向字符串起始位置的常量迭代器
            *
            * * - `cend()`: 返回指向字符串末尾（终止符前）的常量迭代器
            *
            * * - `rbegin()`: 返回指向字符串最后一个字符的反向迭代器
            *
            * * - `rend()`: 返回指向字符串第一个字符前的反向迭代器
            *
            * * - `crbegin()`: 返回指向字符串最后一个字符的常量反向迭代器
            *
            * * - `crend()`: 返回指向字符串第一个字符前的常量反向迭代器

            * 容量相关方法:

            * * - `empty()`: 判断字符串是否为空（长度为 0）
            *
            * * - `size()`: 返回字符串当前长度
            *
            * * - `capacity()`: 返回字符串当前容量
            *
            * * - `resize()`: 调整字符串长度，不足部分用指定字符填充
            *
            * * - `reserve()`: 预分配指定容量的内存，不改变字符串长度

            * 元素访问方法:

            * * - `c_str()`: 返回指向 C 风格字符串（以 '\0' 结尾）的指针
            *
            * * - `back()`: 返回字符串最后一个字符
            *
            * * - `front()`: 返回字符串第一个字符
            *
            * * - `operator[]`: 通过索引访问字符（支持读写和只读版本）

            * 构造函数:

            * * - 默认构造函数: 初始化空字符串
            *
            * * - 从 C 风格字符串构造: 拷贝传入的常量字符串
            *
            * * - 移动构造函数（C 字符串）: 接管传入的临时字符数组所有权
            *
            * * - 拷贝构造函数: 深拷贝另一个字符串对象
            *
            * * - 移动构造函数（字符串对象）: 接管另一个临时字符串的资源
            *
            * * - 初始化列表构造: 从字符初始化列表构造字符串

            * 析构函数:

            * * - 释放字符数组内存，重置成员变量

            * 字符串修改方法:

            * * - `push_back()`: 向字符串末尾添加单个字符、另一个字符串或 C 风格字符串
            *
            * * - `insert_sub_string()`: 在指定位置插入子字符串
            *
            * * - `prepend()`: 在字符串开头插入子字符串
            *
            * * - `swap()`: 与另一个字符串交换内容
            *
            * * - `allocate_resources()`: 重新分配内存以扩展容量
            *
            * * - `pop_back()`: 移除字符串最后一个字符（未显示实现，推测相关）

            * 字符串处理方法:

            * * - `uppercase()`: 将字符串转换为大写
            *
            * * - `lowercase()`: 将字符串转换为小写
            *
            * * - `sub_string()`: 提取从指定位置开始或指定范围的子字符串
            *
            * * - `reverse()`: 返回字符串的反转版本
            *
            * * - `reverse_sub_string()`: 返回指定范围子字符串的反转版本
            *
            * * - `string_print()`: 输出字符串内容到标准输出
            *
            * * - `string_reverse_print()`: 反向输出字符串内容到标准输出

            * 运算符重载:

            * * - `operator=`: 赋值运算符（支持拷贝赋值和移动赋值）
            *
            * * - `operator+=`: 字符串拼接赋值
            *
            * * - `operator+`: 字符串拼接，返回新字符串
            *
            * * - `operator==`: 判断两个字符串是否相等
            *
            * * - `operator<`: 判断当前字符串是否小于另一个字符串（字典序）
            *
            * * - `operator>`: 判断当前字符串是否大于另一个字符串（字典序）
            *
            * * - 友元 `operator<<`: 输出字符串到流
            *
            * * - 友元 `operator>>`: 从流读取字符串

            * 特性:

            * * - 支持移动语义，减少不必要的内存拷贝
            *
            * * - 包含异常处理，对越界访问等情况抛出异常
            *
            * * - 自动扩容机制，容量不足时翻倍扩展（初始容量为 2）
            *
            * * - 所有字符串以 '\0' 结尾，兼容 C 风格字符串操作

            * 注意事项:

            * * - 字符串操作可能抛出 `fault` 异常（如越界访问）
            *
            * * - 浮点类型转换为字符串时可能丢失精度（相关哈希函数中）
            *
            * * - 与标准库 `std::string` 接口类似，但实现细节可能不同
            *
            * * - 移动构造后原字符串对象会被重置（内部指针为 nullptr）

            * 详细请参考 https://github.com/Hatedatastructures/Custom-libraries/blob/main/template_container.md
    */
    class string
    {
    private:
        char *data_;
        uint64_t size_;
        uint64_t capacity_;

    public:
        using iterator = char *;
        using const_iterator = const char *;
        using reverse_iterator = iterator;
        using const_reverse_iterator = const_iterator;
        constexpr static const uint64_t nops = -1;
        [[nodiscard]] iterator begin() const noexcept
        {
            return data_;
        }

        [[nodiscard]] iterator end() const noexcept
        {
            return data_ + size_;
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return static_cast<const_iterator>(data_);
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return static_cast<const_iterator>(data_ + size_);
        }

        [[nodiscard]] reverse_iterator rbegin() const noexcept
        {
            return empty() ? static_cast<reverse_iterator>(end()) : static_cast<reverse_iterator>(end() - 1);
        }

        [[nodiscard]] reverse_iterator rend() const noexcept
        {
            return empty() ? static_cast<reverse_iterator>(begin()) : static_cast<reverse_iterator>(begin() - 1);
        }

        [[nodiscard]] const_reverse_iterator crbegin() const noexcept
        {
            return static_cast<const_reverse_iterator>(cend() - 1);
        }

        [[nodiscard]] const_reverse_iterator crend() const noexcept
        {
            return static_cast<const_reverse_iterator>(cbegin() - 1);
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] uint64_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] uint64_t capacity() const noexcept
        {
            return capacity_;
        }

        [[nodiscard]] const char *c_str() const noexcept
        {
            return static_cast<const char *>(data_);
        } // 返回C风格字符串

        [[nodiscard]] char back() const noexcept
        {
            return size_ > 0 ? data_[size_ - 1] : '\0';
        }

        [[nodiscard]] char front() const noexcept
        {
            return data_[0];
        } // 返回头字符

        string(const char *str_data = " ")
                : size_(str_data == nullptr ? 0 : strlen(str_data)), capacity_(size_)
        {
            // 传进来的字符串是常量字符串，不能直接修改，需要拷贝一份，并且常量字符串在数据段(常量区)浅拷贝会导致程序崩溃
            if (str_data != nullptr)
            {
                data_ = new char[capacity_ + 1];
                std::strncpy(data_, str_data, std::strlen(str_data));
                data_[size_] = '\0';
            }
            else
            {
                data_ = new char[1];
                data_[0] = '\0';
            }
        }
        string(char *&&str_data) noexcept
                : data_(nullptr), size_(str_data == nullptr ? 0 : strlen(str_data)), capacity_(size_)
        {
            // 移动构造函数，拿传入对象的变量初始化本地变量，对于涉及开辟内存的都要深拷贝
            if (str_data != nullptr)
            {
                data_ = str_data;
                str_data = nullptr;
            }
            else
            {
                data_ = new char[1];
                data_[0] = '\0';
            }
        }
        string(const string &str_data)
                : data_(nullptr), size_(str_data.size_), capacity_(str_data.capacity_)
        {
            // 拷贝构造函数，拿传入对象的变量初始化本地变量，对于涉及开辟内存的都要深拷贝
            uint64_t capacity = str_data.capacity_;
            data_ = new char[capacity + 1];
            // algorithm::copy(data_,data_+capacity,str_data.data_); const对象出错
            std::strcpy(data_, str_data.data_);
        }
        string(string &&str_data) noexcept
                : data_(nullptr), size_(str_data.size_), capacity_(str_data.capacity_)
        {
            // 移动构造函数，拿传入对象的变量初始化本地变量，对于涉及开辟内存的都要深拷贝
            //  template_container::algorithm::swap(str_data.data_,data_);
            data_ = str_data.data_;
            size_ = str_data.size_;
            capacity_ = str_data.capacity_;
            str_data.data_ = nullptr;
        }
        string(const std::initializer_list<char> str_data)
        {
            // 初始化列表构造函数
            size_ = str_data.size();
            capacity_ = size_;
            data_ = new char[capacity_ + 1];
            standard_con::algorithm::copy(str_data.begin(), str_data.end(), data_);
            data_[size_] = '\0';
        }
        ~string() noexcept
        {
            delete[] data_;
            data_ = nullptr;
            capacity_ = size_ = 0;
        }
        string &uppercase() noexcept
        {
            // 字符串转大写
            for (string::iterator start_position = data_; start_position != data_ + size_; start_position++)
            {
                if (*start_position >= 'a' && *start_position <= 'z')
                {
                    *start_position -= 32;
                }
            }
            return *this;
        }
        string &lowercase() noexcept
        {
            // 字符串转小写
            for (string::iterator start_position = data_; start_position != data_ + size_; start_position++)
            {
                if (*start_position >= 'A' && *start_position <= 'Z')
                {
                    *start_position += 32;
                }
            }
            return *this;
        }
        // uint64_t str_substring_kmp(const char*& sub_string)
        // {
        //     //查找子串
        // }
        string &prepend(const char *sub_string)
        {
            // 前duan插入子串
            uint64_t len = strlen(sub_string);
            uint64_t new_size = size_ + len;
            allocate_resources(new_size);
            char *temporary_buffers = new char[capacity_ + 1];
            // 临时变量
            memmove(temporary_buffers, data_, size_ + 1);
            memmove(data_, sub_string, len);
            memmove(data_ + len, temporary_buffers, size_ + 1);
            // 比memcpy更安全，memcpy会覆盖原有数据，memmove会先拷贝到临时变量再拷贝到目标地址
            size_ = new_size;
            data_[size_] = '\0';
            delete[] temporary_buffers;
            return *this;
        }
        string &insert_sub_string(const char *sub_string, const uint64_t &start_position)
        {
            try
            {
                // 中间位置插入子串
                if (start_position > size_)
                {
                    throw custom_exception::fault("传入参数位置越界", "insert_sub_string", __LINE__);
                }
                uint64_t len = strlen(sub_string);
                uint64_t new_size = size_ + len;
                allocate_resources(new_size);
                char *temporary_buffers = new char[new_size + 1];
                // 临时变量
                memmove(temporary_buffers, data_, size_ + 1);
                // 从oid_pos开始插入
                memmove(data_ + start_position + len, temporary_buffers + start_position, size_ - start_position + 1);
                memmove(data_ + start_position, sub_string, len);
                size_ = new_size;
                data_[size_] = '\0';
                delete[] temporary_buffers;
                return *this;
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
        }
        [[nodiscard]] string sub_string(const uint64_t &start_position) const
        {
            // 提取字串到'\0'
            try
            {
                if (start_position > size_)
                {
                    throw custom_exception::fault("传入参数位置越界", "sub_string", __LINE__);
                }
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
            string result;
            uint64_t sub_len = size_ - start_position;
            result.allocate_resources(sub_len);
            std::strncpy(result.data_, data_ + start_position, sub_len);
            result.size_ = sub_len;
            result.data_[result.size_] = '\0';
            return result;
        }
        [[nodiscard]] string sub_string_from(const uint64_t &start_position) const
        {
            // 提取字串到末尾
            try
            {
                if (start_position > size_)
                {
                    throw custom_exception::fault("传入参数位置越界", "sub_string_from", __LINE__);
                }
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
            string result;
            uint64_t sub_len = size_ - start_position;
            result.allocate_resources(sub_len);
            std::strncpy(result.data_, data_ + start_position, sub_len);
            result.size_ = sub_len;
            result.data_[result.size_] = '\0';
            return result;
        }
        [[nodiscard]] string sub_string(const uint64_t &start_position, const uint64_t &terminate_position) const
        {
            // 提取字串到指定位置
            try
            {
                if (start_position > size_ || terminate_position > size_ || start_position > terminate_position)
                {
                    throw custom_exception::fault("传入参数位置越界", "sub_string", __LINE__);
                }
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
            string result;
            uint64_t sub_len = terminate_position - start_position;
            result.allocate_resources(sub_len);
            // strncpy更安全
            std::strncpy(result.data_, data_ + start_position, sub_len);
            result.size_ = sub_len;
            result.data_[result.size_] = '\0';
            return result;
        }
        void allocate_resources(const uint64_t &new_inaugurate_capacity)
        {
            // 检查string空间大小，来分配内存
            if (new_inaugurate_capacity <= capacity_)
            {
                // 防止无意义频繁拷贝
                return;
            }
            char *temporary_str_array = new char[new_inaugurate_capacity + 1];
            std::memcpy(temporary_str_array, data_, size_ + 1);

            temporary_str_array[size_] = '\0';
            delete[] data_;
            data_ = temporary_str_array;
            capacity_ = new_inaugurate_capacity;
        }
        string &push_back(const char &temporary_str_data)
        {
            if (size_ == capacity_)
            {
                uint64_t newcapacity = capacity_ == 0 ? 2 : capacity_ * 2;
                allocate_resources(newcapacity);
            }
            data_[size_] = temporary_str_data;
            ++size_;
            data_[size_] = '\0';
            return *this;
        }
        string &push_back(const string &temporary_string_data)
        {
            uint64_t len = size_ + temporary_string_data.size_;
            if (len > capacity_)
            {
                uint64_t new_container_capacity = len;
                allocate_resources(new_container_capacity);
            }
            std::strncpy(data_ + size_, temporary_string_data.data_, temporary_string_data.size());
            size_ = size_ + temporary_string_data.size_;
            data_[size_] = '\0';
            return *this;
        }
        string &push_back(const char *temporary_str_ptr_data)
        {
            if (temporary_str_ptr_data == nullptr)
            {
                return *this;
            }
            const uint64_t len = strlen(temporary_str_ptr_data);
            const uint64_t new_container_capacity = len + size_;
            if (new_container_capacity > capacity_)
            {
                allocate_resources(new_container_capacity);
            }
            std::strncpy(data_ + size_, temporary_str_ptr_data, len);
            size_ = size_ + len;
            data_[size_] = '\0';
            return *this;
        }
        string &resize(const uint64_t &inaugurate_size, const char &default_data = '\0')
        {
            // 扩展字符串长度
            if (inaugurate_size > capacity_)
            {
                // 长度大于容量，重新开辟内存
                try
                {
                    allocate_resources(inaugurate_size);
                }
                catch (const std::bad_alloc &new_charptr_abnormal)
                {
                    std::cerr << new_charptr_abnormal.what() << std::endl;
                    throw;
                }
                for (string::iterator start_position = data_ + size_; start_position != data_ + inaugurate_size; start_position++)
                {
                    *start_position = default_data;
                }
                size_ = inaugurate_size;
                data_[size_] = '\0';
            }
            else
            {
                // 如果新长度小于当前字符串长度，直接截断放'\0'
                size_ = inaugurate_size;
                data_[size_] = '\0';
            }
            return *this;
        }
        iterator reserve(const uint64_t &new_container_capacity)
        {
            try
            {
                allocate_resources(new_container_capacity);
            }
            catch (const std::bad_alloc &new_charptr_abnormal)
            {
                std::cerr << new_charptr_abnormal.what() << std::endl;
                throw;
            }
            return data_;
            // 返回首地址迭代器
        }
        string &swap(string &str_data) noexcept
        {
            standard_con::algorithm::swap(data_, str_data.data_);
            standard_con::algorithm::swap(size_, str_data.size_);
            standard_con::algorithm::swap(capacity_, str_data.capacity_);
            return *this;
        }
        [[nodiscard]] string reverse() const
        {
            try
            {
                if (size_ == 0)
                {
                    throw custom_exception::fault("当前string为空", "reserve", __LINE__);
                }
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
            string reversed_string;
            for (string::const_reverse_iterator reverse = rbegin(); reverse != rend(); reverse--)
            {
                reversed_string.push_back(*reverse);
            }
            return reversed_string;
        }
        [[nodiscard]] string reverse_sub_string(const uint64_t &start_position, const uint64_t &terminate_position) const
        {
            try
            {
                if (start_position > size_ || terminate_position > size_ || start_position > terminate_position || size_ == 0)
                {
                    throw custom_exception::fault("string回滚位置异常", "reverse_sub_string", __LINE__);
                }
            }
            catch (const custom_exception::fault &process)
            {
                std::cerr << process.what() << " " << process.function_name_get() << " " << process.line_number_get() << std::endl;
                throw;
            }
            string reversed_result;
            for (string::const_reverse_iterator reverse = data_ + terminate_position - 1; reverse != data_ + start_position - 1; reverse--)
            {
                reversed_result.push_back(*reverse);
            }
            return reversed_result;
        }
        void string_print() const noexcept
        {
            std::cout << data_ << std::endl;
        }
        void string_reverse_print() const noexcept
        {
            for (string::const_reverse_iterator start_position = crbegin(); start_position != crend(); start_position--)
            {
                std::cout << *start_position;
            }
            std::cout << std::endl;
        }
        friend std::ostream &operator<<(std::ostream &string_ostream, const string &str_data);
        friend std::istream &operator>>(std::istream &string_istream, string &str_data);
        string &operator=(const string &str_data)
        {
            try
            {
                if (this != &str_data) // 防止无意义拷贝
                {
                    delete[] data_;
                    uint64_t capacity = str_data.capacity_;
                    data_ = new char[capacity + 1];
                    std::strncpy(data_, str_data.data_, str_data.size());
                    capacity_ = str_data.capacity_;
                    size_ = str_data.size_;
                    data_[size_] = '\0';
                }
            }
            catch (const std::bad_alloc &process)
            {
                std::cerr << process.what() << std::endl;
                throw;
            }
            return *this;
        }
        string &operator=(const char *str_data)
        {
            try
            {
                delete[] data_;
                uint64_t capacity = strlen(str_data);
                data_ = new char[capacity + 1];
                std::strncpy(data_, str_data, strlen(str_data));
                capacity_ = capacity;
                size_ = capacity;
                data_[size_] = '\0';
            }
            catch (const std::bad_alloc &process)
            {
                std::cerr << process.what() << std::endl;
                throw;
            }
            return *this;
        }
        string &operator=(string &&str_data) noexcept
        {
            if (this != &str_data)
            {
                delete[] data_;
                size_ = str_data.size_;
                capacity_ = str_data.capacity_;
                data_ = str_data.data_;
                str_data.data_ = nullptr;
            }
            return *this;
        }
        string &operator+=(const string &str_data)
        {
            uint64_t len = size_ + str_data.size_;
            allocate_resources(len);
            std::strncpy(data_ + size_, str_data.data_, str_data.size());
            size_ = size_ + str_data.size_;
            data_[size_] = '\0';
            return *this;
        }
        bool operator==(const string &str_data) const noexcept
        {
            if (size_ != str_data.size_)
            {
                return false;
            }
            for (uint64_t compare_traversal = 0; compare_traversal < size_; compare_traversal++)
            {
                if (data_[compare_traversal] != str_data.data_[compare_traversal])
                {
                    return false;
                }
            }
            return true;
        }
        bool operator<(const string &str_data) const noexcept
        {
            uint64_t min_len = size_ < str_data.size_ ? size_ : str_data.size_;
            for (uint64_t compare_traversal = 0; compare_traversal < min_len; compare_traversal++)
            {
                if (data_[compare_traversal] != str_data.data_[compare_traversal])
                {
                    return data_[compare_traversal] < str_data.data_[compare_traversal];
                }
            }
            return size_ < str_data.size_;
        }
        bool operator>(const string &str_data) const noexcept
        {
            uint64_t min_len = size_ < str_data.size_ ? size_ : str_data.size_;
            for (uint64_t compare_traversal = 0; compare_traversal < min_len; compare_traversal++)
            {
                if (data_[compare_traversal] != str_data.data_[compare_traversal])
                {
                    return data_[compare_traversal] > str_data.data_[compare_traversal];
                }
            }
            return size_ > str_data.size_;
        }
        char &operator[](const uint64_t &access_location)
        {
            try
            {
                if (access_location <= size_)
                {
                    return data_[access_location]; // 返回第ergodic_value个元素的引用
                }
                else
                {
                    throw custom_exception::fault("越界访问", "string::operator[]", __LINE__);
                }
            }
            catch (const custom_exception::fault &access_exception)
            {
                std::cerr << access_exception.what() << " " << access_exception.function_name_get() << " " << access_exception.line_number_get() << std::endl;
                throw;
            }
            // 就像data_在外面就能访问它以及它的成员，所以这种就可以理解成出了函数作用域还在，进函数之前也能访问的就是引用
        }
        const char &operator[](const uint64_t &access_location) const
        {
            try
            {
                if (access_location <= size_)
                {
                    return data_[access_location]; // 返回第ergodic_value个元素的引用
                }
                else
                {
                    throw custom_exception::fault("越界访问", "string::operator[]const", __LINE__);
                }
            }
            catch (const custom_exception::fault &access_exception)
            {
                std::cerr << access_exception.what() << " " << access_exception.function_name_get() << " " << access_exception.line_number_get() << std::endl;
                throw;
            }
        }
        [[nodiscard]] string operator+(const string &string_array) const
        {
            string return_string_object;
            const uint64_t object_len = size_ + string_array.size_;
            return_string_object.allocate_resources(object_len);
            std::strncpy(return_string_object.data_, data_, size());
            std::strncpy(return_string_object.data_ + size_, string_array.data_, string_array.size());
            return_string_object.size_ = size_ + string_array.size_;
            return_string_object.data_[return_string_object.size_] = '\0';
            return return_string_object; // 不能转为右值，编译器会再做一次优化
        }
    };
    std::istream &operator>>(std::istream &string_istream, string &str_data)
    {
        while (true)
        {
            char single_char = static_cast<char>(string_istream.get()); // gat函数只读取一个字符
            if (single_char == '\n' || single_char == EOF)
            {
                break;
            }
            else
            {
                str_data.push_back(single_char);
            }
        }
        return string_istream;
    }
    inline std::ostream &operator<<(std::ostream &string_ostream, const string &str_data)
    {
        for (const char start_position : str_data)
        {
            string_ostream << start_position;
        }
        return string_ostream;
    }
}
namespace standard_con
{
    using string_container::string;
}