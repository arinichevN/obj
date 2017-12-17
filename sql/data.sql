CREATE TABLE "prog"
(
  "id" INTEGER PRIMARY KEY,
  "heater_id" INTEGER NOT NULL,--heater_id and cooler_id shall be unique
  "cooler_id" INTEGER NOT NULL,--heater_id and cooler_id shall be unique
  "ambient_temperature" REAL NOT NULL,
  "matter_mass" REAL NOT NULL,
  "matter_ksh" REAL NOT NULL,--specific heat
  "loss_factor" REAL NOT NULL,-- >= 0 
  "temperature_pipe_length" INTEGER NOT NULL,

  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
CREATE UNIQUE INDEX "hc_ind" on prog (heater_id ASC, cooler_id ASC);
