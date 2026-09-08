#include "storage.h"
#include "yellow.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t save_bytes[0x8000];
extern FILE *test_disk;
extern const char *test_fault;
extern uint8_t test_stage;
extern unsigned test_writes;
static uint8_t before[32768];
static YellowPokemonView mon;

static uint16_t last_completed[ST_PREPARE + 1];
static uint8_t completed_stages[ST_PREPARE + 1];
static void progress(uint8_t stage, uint16_t completed, uint16_t total) {
  test_stage = stage;
  if (total) {
    assert(completed <= total);
    if (!completed)
      last_completed[stage] = 0;
    assert(completed >= last_completed[stage]);
    last_completed[stage] = completed;
    if (completed == total)
      ++completed_stages[stage];
  }
}
static void load_fixture(const char *path) {
  FILE *f = fopen(path, "rb");
  assert(f);
  assert(fread(save_bytes, 1, 32768, f) == 32768);
  fclose(f);
  memcpy(before, save_bytes, 32768);
  assert(yellow_init() == YE_OK);
}
static void verify_checksums(void) {
  uint32_t sum = 0;
  unsigned i;
  for (i = 0x2598; i < 0x3523; ++i)
    sum += save_bytes[i];
  assert(save_bytes[0x3523] == (uint8_t)~sum);
}
static void core_checks(const char *path) {
  uint8_t id, qty, name[11];
  unsigned i;
  load_fixture(path);
  assert(yellow_is_yellow_hint());
  yellow_flush();
  assert(memcmp(before, save_bytes, 32768) == 0);
  assert(yellow_set_level(0, 0, 50) == YE_OK);
  yellow_get_pokemon(0, 0, &mon);
  assert(mon.level == 50 && mon.exp == 125000 && mon.max_hp > 20);
  assert(mon.current_hp == mon.max_hp);
  assert(yellow_set_dvs(0, 0, 15, 15, 15, 15) == YE_OK);
  assert(yellow_set_stat_exp(0, 0, YELLOW_STAT_HP, 65535) == YE_OK);
  yellow_get_pokemon(0, 0, &mon);
  assert(mon.max_hp == 141);
  assert(yellow_set_species(0, 0, yellow_dex_to_index(1)) == YE_OK);
  yellow_get_pokemon(0, 0, &mon);
  assert(mon.dex == 1 && mon.level == 50 && mon.exp == 117360);
  assert(mon.types[0] == 22 && mon.types[1] == 3);
  assert(yellow_set_exp(0, 0, yellow_exp_for_level(3, 25)) == YE_OK);
  yellow_get_pokemon(0, 0, &mon);
  assert(mon.level == 25);
  assert(yellow_set_move(0, 0, 0, 45) == YE_OK);
  assert(yellow_set_pp_ups(0, 0, 0, 3) == YE_OK);
  assert(yellow_max_pp(40, 3) == 61);
  assert(yellow_set_move_pp(0, 0, 0, 61) == YE_OK);
  yellow_get_pokemon(0, 0, &mon);
  assert((mon.pp[0] & 63) == 61 && mon.pp[0] >> 6 == 3);
  assert(yellow_encode_text("SPARK", name, 0) == YE_OK);
  assert(yellow_set_nickname(0, 0, name) == YE_OK);
  assert(yellow_set_money(999999) == YE_OK && yellow_get_money() == 999999);
  yellow_set_badges(0x81);
  assert(yellow_get_badges() == 0x81);
  yellow_set_pikachu_friendship(255);
  assert(yellow_pikachu_friendship() == 255);
  assert(yellow_bag_add(1, 99) == YE_OK);
  assert(yellow_bag_count() == 2);
  assert(yellow_bag_remove(0) == YE_OK);
  yellow_bag_get(0, &id, &qty);
  assert(id == 1 && qty == 99);
  yellow_flush();
  verify_checksums();
  assert(memcmp(before, save_bytes, 0x2000) == 0);
  assert(memcmp(before + 0x4000, save_bytes + 0x4000, 0x4000) == 0);
  for (i = 0; i < 12; ++i) {
    load_fixture(path);
    assert(yellow_set_level(YELLOW_LOC_BOX(i), 0, 80) == YE_OK);
    yellow_get_pokemon(YELLOW_LOC_BOX(i), 0, &mon);
    assert(mon.level == 80 && mon.exp == 512000);
    yellow_flush();
    verify_checksums();
    assert(yellow_init() == YE_OK);
    assert(!(yellow_status_flags() &
             (YE_F_BANK2_CHECKSUM | YE_F_BANK3_CHECKSUM | YE_F_BOX_CHECKSUM)));
    if (i == 0)
      assert(memcmp(before + 0x4000, save_bytes + 0x4000, 0x4000) == 0);
    else
      assert(memcmp(before + 0x2598, save_bytes + 0x2598, 0x3524 - 0x2598) ==
             0);
  }
  load_fixture(path);
  save_bytes[0x284c] = 0;
  {
    uint8_t sum = 0;
    for (i = 0x2598; i < 0x3523; ++i)
      sum += save_bytes[i];
    save_bytes[0x3523] = (uint8_t)~sum;
  }
  assert(yellow_init() == YE_OK);
  assert(yellow_box_count(0) == 1 && yellow_box_count(1) == 0);
  assert(yellow_set_level(1, 0, 10) == YE_OK);
  load_fixture(path);
  save_bytes[0x2598] ^= 1;
  assert(yellow_init() == YE_OK);
  assert(yellow_status_flags() & YE_F_MAIN_CHECKSUM);
  load_fixture(path);
  save_bytes[0x2f2c] = 7;
  assert(yellow_init() == YE_ERR_MALFORMED_SAVE);
  memcpy(before, save_bytes, 32768);
  assert(yellow_set_level(0, 0, 100) != YE_OK);
  assert(memcmp(before, save_bytes, 32768) == 0);
  load_fixture(path);
  assert(yellow_set_level(0, 0, 0) != YE_OK);
  assert(yellow_set_species(0, 0, 0) != YE_OK);
  assert(yellow_set_move(0, 0, 0, 166) != YE_OK);
  assert(yellow_set_money(1000000UL) != YE_OK);
  assert(yellow_bag_add(7, 1) == YE_ERR_BAD_ITEM);
  assert(yellow_bag_add(21, 1) == YE_ERR_BAD_ITEM);
  assert(yellow_bag_add(84, 1) == YE_ERR_BAD_ITEM);
  assert(yellow_bag_add(255, 1) == YE_ERR_BAD_ITEM);
  memset(name, 0x80, sizeof(name));
  assert(yellow_set_nickname(0, 0, name) == YE_ERR_TEXT_TOO_LONG);
  assert(memcmp(before, save_bytes, 32768) == 0);
  puts("PASS core: party/12 boxes, stat coherence, checksums, untouched "
       "regions, invalid-input refusal");
}

int main(int argc, char **argv) {
  uint8_t result;
  assert(argc >= 3);
  if (!strcmp(argv[1], "core")) {
    core_checks(argv[2]);
    return 0;
  }
  test_disk = fopen(argv[2], "r+b");
  assert(test_disk);
  if (argc > 3)
    test_fault = argv[3];
  assert(storage_mount(progress) == STORE_OK);
  if (!strcmp(argv[1], "list")) {
    char name[256];
    uint8_t i;
    assert(storage_list("/", 0, progress) == STORE_OK);
    for (i = 0; i < storage_count; ++i) {
      storage_name(i, name);
      printf("%s|%s\n", storage_entries[i].name, name);
    }
    assert(test_writes == 0);
    fclose(test_disk);
    return 0;
  }
  assert(storage_list("/", 0, progress) == STORE_OK);
  assert(storage_count == 1 && !strcmp(storage_entries[0].name, "YELLOW.SRM"));
  result = storage_load("/YELLOW.SRM", progress);
  assert(result == STORE_OK);
  assert(yellow_init() == YE_OK);
  assert(yellow_set_level(0, 0, 50) == YE_OK);
  yellow_flush();
  result = storage_commit(progress);
  if (result) {
    assert(!completed_stages[ST_FINISHED]);
    if (!strcmp(test_fault, "backup-write"))
      assert(!completed_stages[ST_BACKUP]);
    if (!strcmp(test_fault, "backup-corrupt"))
      assert(!completed_stages[ST_VERIFY_BACKUP]);
    if (!strcmp(test_fault, "save-write"))
      assert(!completed_stages[ST_WRITE]);
  } else {
    assert(completed_stages[ST_LOAD] == 1);
    assert(completed_stages[ST_CHECK_SOURCE] == 2);
    assert(completed_stages[ST_BACKUP] == 1);
    assert(completed_stages[ST_VERIFY_BACKUP] == 1);
    assert(completed_stages[ST_WRITE] == 1);
    assert(completed_stages[ST_VERIFY_SAVE] == 1);
    assert(completed_stages[ST_FINISHED] == 1);
  }
  printf("result=%u stage=%u writes=%u backup=%s\n", result, storage_stage,
         test_writes, storage_backup);
  fclose(test_disk);
  return 0;
}
