#ifndef CARDS_H
#define CARDS_H

struct card_s {
	const char *title;
};

struct card_hand_s {
	struct card_s *cards;
};

// cards
void card_init(struct card_s *);
void card_destroy(struct card_s *);

// card hand
void card_hand_init(struct card_s *);
void card_hand_destroy(struct card_s *);

#endif

