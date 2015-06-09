all: rescurejpg
rescurejpg: rescuejpg.c
    gcc $@ $^
