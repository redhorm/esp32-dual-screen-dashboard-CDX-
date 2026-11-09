// Test-only HTTP bridge companion: executes the actual firmware state model.
#include "core/storm_protocol.h"
#include <iostream>
#include <string>
int main() {
  Storm::State state;
  uint32_t now=0,value=0; int action=-1;
  while(std::cin >> now >> action >> value) {
    if(action>=0 && action<int(Storm::Action::Count) &&
       Storm::validValue(static_cast<Storm::Action>(action),value)) {
      Storm::Command command;command.action=static_cast<Storm::Action>(action);
      command.value=value;state.apply(command,now);
    } else state.tick(now);
    auto &s=state.s;auto &w=s.wow;
    std::cout << std::boolalpha << "{\"mode\":" << unsigned(s.mode)
      << ",\"session\":" << s.session << ",\"running\":" << s.running
      << ",\"elapsed\":" << s.elapsed << ",\"gates\":" << s.gates
      << ",\"penalties\":" << s.penalties << ",\"score\":" << s.score
      << ",\"weight\":" << s.weight << ",\"gameMs\":" << s.gameMs
      << ",\"hits\":" << s.hits << ",\"paused\":" << s.gamePaused
      << ",\"done\":" << s.gameDone << ",\"best\":" << s.best
      << ",\"history\":0,\"saved\":false,\"wow\":{\"ms\":" << w.ms
      << ",\"elapsed\":" << w.elapsed << ",\"score\":" << w.score
      << ",\"best\":" << w.best << ",\"hits\":" << w.hits
      << ",\"paused\":" << w.paused << ",\"done\":" << w.done
      << ",\"audioActive\":" << w.audioActive << ",\"seed\":" << w.seed
      << ",\"exploded\":" << w.exploded << ",\"bands\":["
      << unsigned(w.bands[0]) << ',' << unsigned(w.bands[1]) << ','
      << unsigned(w.bands[2]) << ',' << unsigned(w.bands[3])
      << "]},\"debug\":{\"car\":" << w.car << ",\"gazeX\":" << w.gazeX
      << ",\"energy\":" << w.energy << ",\"yaw\":" << w.yaw
      << "}}" << std::endl;
  }
}
