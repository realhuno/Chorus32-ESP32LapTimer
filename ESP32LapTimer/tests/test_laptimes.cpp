#define BOOST_TEST_MODULE LapTest
#include <boost/test/included/unit_test.hpp>

#include "compatability.h"

#define MAX_LAPS_NUM 20
#define MAX_NUM_PILOTS 2

#define sendLap(x,y)

#include <cstdio>
#include <iostream>

#include "Laptime.h"

void print_laps(int start_lap, int max_lap) {
  printf("Laps from %d to %d: ", start_lap, max_lap);

  for(int i = start_lap; i < max_lap; ++i) {
    printf("%d ", getLaptime(0, i));
  }
  printf("\n");
}

BOOST_AUTO_TEST_CASE(test_lap) {
  resetLaptimes();
  BOOST_REQUIRE_EQUAL(getLaptime(0, 0), 0);
  addLap(0, 1);
  BOOST_REQUIRE_EQUAL(getLaptime(0, 1), 1);

  for(int i = 2; i < 40; ++i) {
    addLap(0, i);
    print_laps(0, 20);
    //BOOST_REQUIRE_EQUAL(getLaptime(0, i), i);
    //BOOST_REQUIRE_EQUAL(getLaptime(1, 0), 0);
    //BOOST_REQUIRE_EQUAL(getLaptime(0, i-1), i-1);
  }


}



