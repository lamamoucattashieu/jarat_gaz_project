#include <gtest/gtest.h>

extern "C" {
#include "common.h"
#include "util.h"
#include "gps.h"
#include "protocol.h"
}

TEST(DistanceTest, ZeroDistance) {
    double d0 = haversine_km(0, 0, 0, 0);
    EXPECT_DOUBLE_EQ(d0, 0.0);
}

TEST(DistanceTest, PositiveDistance) {
    double d = haversine_km(31.956, 35.945, 31.957, 35.945);
    EXPECT_GT(d, 0.0);
}

TEST(ProtocolTest, HeartbeatRoundTrip) {
    char buf[256];
    TruckInfo t;
    time_t ts;

    int n = format_hb(buf, sizeof(buf), "TRK12", 31.0, 35.0, 6012, 123);
    ASSERT_GT(n, 0);
    ASSERT_TRUE(parse_hb(buf, &t, &ts));

    EXPECT_STREQ(t.id, "TRK12");
    EXPECT_DOUBLE_EQ(t.lat, 31.0);
    EXPECT_DOUBLE_EQ(t.lon, 35.0);
    EXPECT_EQ(t.tcp_port, 6012);
}

TEST(ProtocolTest, PingRoundTrip) {
    char buf[256];
    PingMsg p{};
    strcpy(p.truck_id, "TRK12");
    strcpy(p.user_id, "USR1");
    strcpy(p.addr, "12 St");
    strcpy(p.note, "2 cyl");

    int n = format_ping(buf, sizeof(buf), &p);
    ASSERT_GT(n, 0);

    PingMsg p2{};
    ASSERT_TRUE(parse_ping(buf, &p2));
    EXPECT_STREQ(p2.truck_id, "TRK12");
    EXPECT_STREQ(p2.user_id, "USR1");
    EXPECT_STREQ(p2.addr, "12 St");
    EXPECT_STREQ(p2.note, "2 cyl");
}

TEST(GpsTest, MovesOverTime) {
    double lat = 31.956;
    double lon = 35.945;
    gps_init(lat, lon, 5.0);
    double lat0 = lat, lon0 = lon;

    for (int i = 0; i < 1000; ++i) {
        gps_step(&lat, &lon);
    }
    // Not a strict check, but should usually move
    EXPECT_FALSE(lat == lat0 && lon == lon0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
