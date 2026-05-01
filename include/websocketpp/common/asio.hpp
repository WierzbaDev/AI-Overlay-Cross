#ifndef WEBSOCKETPP_COMMON_ASIO_HPP
#define WEBSOCKETPP_COMMON_ASIO_HPP

#ifdef ASIO_STANDALONE
  #include <asio.hpp>
  #include <asio/deadline_timer.hpp>
  #include <boost/date_time/posix_time/posix_time_types.hpp>
#else
  #include <boost/asio.hpp>
  #include <boost/asio/deadline_timer.hpp>
#endif

namespace websocketpp {
namespace lib {

#ifdef ASIO_STANDALONE
namespace asio {
    using namespace ::asio;
    using std::errc;
    using io_service = ::asio::io_context;
    using steady_timer = ::asio::deadline_timer;

    template <typename T>
    bool is_neg(T duration) {
        return duration.is_negative();
    }

    inline boost::posix_time::time_duration milliseconds(long duration) {
        return boost::posix_time::milliseconds(duration);
    }
} // namespace asio
#else
namespace asio {
    using namespace boost::asio;
    using boost::system::error_code;
    namespace errc = boost::system::errc;
    using io_service = boost::asio::io_context;
    using steady_timer = boost::asio::deadline_timer;

    template <typename T>
    bool is_neg(T duration) {
        return duration.is_negative();
    }

    inline boost::posix_time::time_duration milliseconds(long duration) {
        return boost::posix_time::milliseconds(duration);
    }
} // namespace asio
#endif

} // namespace lib
} // namespace websocketpp

#endif // WEBSOCKETPP_COMMON_ASIO_HPP
