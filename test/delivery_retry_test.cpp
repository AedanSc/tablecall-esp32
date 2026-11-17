#include "delivery_retry.h"
#include "watch_queue.h"
#include <assert.h>
#include <stdio.h>
using namespace service;
Packet message() {
  Packet p{}; p.magic=MAGIC; p.version=VERSION; p.kind=Kind::Request;
  p.table=1; p.session=17; p.sequence=8; p.request=Request::Water;
  p.status=Status::Accepted; p.sender[0]=9; return p;
}
int main() {
  DeliveryRetry retry; retry.restart(100);
  uint32_t now=100;
  for (int attempt=0; attempt<5; ++attempt) {
    assert(retry.due(now)); retry.sent(now,false,0);
    assert(retry.retryAt-now == (attempt<4 ? 450U : 5000U));
    assert(!retry.due(retry.retryAt-1)); now=retry.retryAt;
  }
  // Link outage lasting minutes must never exhaust retries or overflow a counter.
  for (int attempt=0; attempt<1000; ++attempt) {
    assert(retry.due(now)); retry.sent(now,false,150);
    assert(retry.retryAt-now == 5150 && retry.fastAttempts == 5); now=retry.retryAt;
  }
  TableRequest table; const Packet p=message(); assert(table.start(p));
  WatchQueue watch; assert(watch.add(p,now));
  Packet ack=p; ack.kind=Kind::Ack; assert(table.apply(ack)); retry.acknowledged(now);
  assert(!retry.due(now) && retry.due(now+5000));
  // Watch resets while the table stays powered: periodic replay restores queue.
  watch=WatchQueue{};
  now+=5000; assert(table.active() && table.confirmed && retry.due(now));
  retry.sent(now,table.confirmed,0); assert(watch.add(table.packet,now));
  assert(watch.count==1 && sameRequest(watch.entries[0].packet,p));
  // Normal duplicate replay does not stack new requests or reset queue age.
  assert(!watch.add(table.packet,now+5000) && watch.groups[0].arrived==now);
  Packet coming=p; coming.kind=Kind::State; coming.status=Status::Coming;
  assert(table.apply(coming)); retry.restart(now); // manual repeat press
  assert(retry.due(now) && table.state==Status::Coming && sameRequest(table.packet,p));
  // Old fulfillment of the same request still clears it after replay.
  Packet done=coming; done.status=Status::Fulfilled;
  assert(table.apply(done) && !table.active());
  retry.restart(0xFFFFFF00U); retry.sent(0xFFFFFF00U,true,0);
  assert(!retry.due(0xFFFFFF01U)); assert(retry.due(uint32_t(0xFFFFFF00U+5000U)));
  puts("Delivery retry and watch-reset recovery tests passed.");
}
