/*#pragema once

#LEVEL_MAX_BYTES (128*128*8)

class Level{
  private:
    uint8_t _flash_buffer[LEVEL_MAX_BYTES]={};//content loaded straight from flash lives heres
  public:
    Level();
    void begin();
    void update();
    void end();
};

class LevelUnlock : public Level{

}*/