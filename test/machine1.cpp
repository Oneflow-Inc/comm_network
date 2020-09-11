#include <iostream>
#include <vector>

int main() {
  // create test array
  std::vector<int> test_values(10000, 10);
  // init global ibverbs comm_net
  IBVerbsCommNet::Init();
  // send register ready message
  Global<CommNet>::Get()->SendMsg(dst_machine_id, msg);
}