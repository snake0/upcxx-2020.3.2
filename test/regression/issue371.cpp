#include <upcxx/upcxx.hpp>
#include <iostream>
#include <sstream>
#include <assert.h>

using namespace upcxx;
int main() {
 init();

 // create two singleton teams
 team t1 = world().split(rank_me(), rank_me());
 team t2 = local_team().split(rank_me(), rank_me());
 assert(t1.rank_n() == 1 && t1.rank_me() == 0);
 assert(t2.rank_n() == 1 && t2.rank_me() == 0);

 // and a few clones
 team t3 = world().split(0, rank_me()); // clone of world
 team t4 = local_team().split(0, local_team().rank_me()); // clone of local_team
 assert(t3.rank_n() == rank_n() && t3.rank_me() == rank_me());
 assert(t4.rank_n() == local_team().rank_n() && t4.rank_me() == local_team().rank_me());

 // collect ids
 static team_id id_t1 = t1.id();
 static team_id id_t2 = t2.id();
 static team_id id_t3 = t3.id();
 static team_id id_t4 = t4.id();
 static team_id id_world = world().id();
 static team_id id_local = local_team().id();

 std::ostringstream oss;
 oss << rank_me() << ":" 
     << " id_t1=" << id_t1
     << " \tid_t2=" << id_t2
     << " \tid_t3=" << id_t3
     << " \tid_t4=" << id_t4
     << " \tid_world=" << id_world
     << " \tid_local=" << id_local
     << '\n';
 std::cout << oss.str() << std::flush;

 // confirm local uniqueness
 if (id_t1 == id_t2 || id_t1 == id_t3 || id_t1 == id_t4 || id_t1 == id_world || id_t1 == id_local ||
     id_t2 == id_t3 || id_t2 == id_t4 || id_t2 == id_world || id_t2 == id_local ||
     id_t3 == id_t4 || id_t3 == id_world || id_t3 == id_local ||
     id_t4 == id_world || id_t4 == id_local ||
     id_world == id_local) {
     std::cout << "ERROR on rank " << rank_me() << " ids for distinct teams not locally unique!" << std::endl;
 }

 barrier();

 if (rank_me()) {
   rpc(0, [](intrank_t r, team_id r_t1, team_id r_t2, team_id r_t3, team_id r_t4, team_id r_world, team_id r_local) {
         assert(rank_me() == 0 && r != rank_me());

         // compare rank r's job-wide team_ids to rank 0's
         if (r_world != id_world) std::cout << "ERROR: id_world doesn't match across ranks!" << std::endl;
         if (r_t3 != id_t3) std::cout << "ERROR: id_t3 doesn't match across ranks!" << std::endl;

         // t1 and t2 are singleton teams and should not match anything from a different rank
         if (r_t1 == id_t1 || r_t1 == id_t2 || r_t1 == id_t3 || r_t1 == id_t4 || r_t1 == id_local) 
           std::cout << "ERROR: t1.id() values are not unique across ranks!" << std::endl;
         if (r_t2 == id_t1 || r_t2 == id_t2 || r_t2 == id_t3 || r_t2 == id_t4 || r_t2 == id_local) 
           std::cout << "ERROR: t2.id() values are not unique across ranks!" << std::endl;

         // check local_team() ids reflect actual local_team membership
         if (local_team_contains(r)) { // same local_team
           if (r_local != id_local) 
             std::cout << "ERROR: local_team().id() for ranks 0 and " << r << " should be the same!" << std::endl;
           if (r_t4 != id_t4) 
             std::cout << "ERROR: t4.id() for ranks 0 and " << r << " should be the same!" << std::endl;
         } else { // different local_team
           if (r_local == id_local) 
             std::cout << "ERROR: local_team().id() for ranks 0 and " << r << " should be different!" << std::endl;
           if (r_t4 == id_t4) 
             std::cout << "ERROR: t4.id() for ranks 0 and " << r << " should be different!" << std::endl;
         }

       }, rank_me(), id_t1, id_t2, id_t3, id_t4, id_world, id_local).wait();
 }
 barrier();

 if (!rank_me()) std::cout << "done." << std::endl;
 barrier();

 finalize();
}
