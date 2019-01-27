CREATE TABLE "peer" (
    "id" TEXT NOT NULL PRIMARY KEY,
    "port" INTEGER NOT NULL,
    "ip_addr" TEXT NOT NULL
);
CREATE TABLE "channel"
(
  "id" INTEGER PRIMARY KEY, -- (regulater uses it)
  "heater_id" INTEGER NOT NULL,--heater_id and cooler_id shall be unique (regulater uses it)
  "cooler_id" INTEGER NOT NULL,--heater_id and cooler_id shall be unique (regulater uses it)
  "ambient_temperature" REAL NOT NULL,--matter temperature will aim to it
  "matter_mass" REAL NOT NULL,
  "matter_ksh" REAL NOT NULL,--specific heat
  "loss_factor" REAL NOT NULL,-- >= 0 
  "loss_power" REAL NOT NULL,-- >= 0 
  "temperature_pipe_length" INTEGER NOT NULL,--delay between getting energy and increasing temperature

  "cycle_duration_sec" INTEGER NOT NULL,
  "cycle_duration_nsec" INTEGER NOT NULL,
  "save" INTEGER NOT NULL,
  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
CREATE UNIQUE INDEX "hc_ind" on channel (heater_id ASC, cooler_id ASC);
