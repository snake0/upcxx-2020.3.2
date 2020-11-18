#ifndef _1e82e1a3_aff9_4d11_8728_ae3772c6a6f7
#define _1e82e1a3_aff9_4d11_8728_ae3772c6a6f7

#include <sstream>

namespace upcxx {
namespace backend {
namespace gasnet {
  class noise_log {
    const char *fn_name;
    std::stringstream ss;
    bool empty = true;

  private:
    noise_log() {
      this->fn_name = nullptr;
    }
    
  public:
    static noise_log muted() {
      return noise_log();
    }
    
    noise_log(const char *fn_name) {
      this->fn_name = fn_name;
    }
    noise_log(noise_log &&that):
      ss(std::move(that.ss)) {
      fn_name = that.fn_name;
      empty = that.empty;
    }
    void reset() {
      ss = std::stringstream();
      empty = true;
    }
    
    struct line_type {
      noise_log *parent;

      line_type(noise_log *parent) {
        this->parent = parent;
      }
      line_type(line_type &&that) {
        this->parent = that.parent;
        that.parent = nullptr;
      }
      ~line_type() {
        if(parent)
          parent->ss << '\n';
      }
      
      template<typename T>
      line_type& operator<<(T &&x) {
        parent->ss << std::forward<T>(x);
        return *this;
      }

      line_type& operator<<(std::ostream&(&op)(std::ostream&)) {
        op(parent->ss);
        return *this;
      }
      line_type& operator<<(std::ios_base&(&op)(std::ios_base&)) {
        op(parent->ss);
        return *this;
      }
    };

    line_type line() {
      empty = false;
      ss << "> ";
      return line_type{this};
    }

    line_type warn() {
      return static_cast<line_type&&>(line() << "WARNING: ");
    }

    void show();

    static std::string size(std::size_t x);
  };
}}}
#endif
