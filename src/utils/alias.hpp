namespace fs = std::filesystem;
using Path = fs::path;

template <class T>
using Ref = std::reference_wrapper<T>;

namespace boost {
namespace json {}
}  // namespace boost
namespace json = boost::json;
