/* ----------- Decoder-specific data structures ----------- */

/* Internal state for inflate */
typedef struct {
	/* Input state */
	uint32_t bit_buffer;     /* Bit buffer */
	uint32_t bits_in_buffer; /* Number of bits in buffer */
	uint8_t final_block;     /* Is this the final block? */

	/* Huffman tables */
	huffman_table literals;  /* Literal/length codes */
	huffman_table distances; /* Distance codes */

	/* Current state */
	int state;              /* Current state of processing */
	block_type btype;       /* Current block type */

	/* Output state */
	uint8_t *window;        /* Sliding window for LZ77 */
	uint32_t window_size;   /* Size of window */
	uint32_t window_pos;    /* Current position in window */
} inflate_state;

/* Get a bit from the input stream */

static int get_bit(z_stream *strm, inflate_state *state) {
	if (state->bits_in_buffer == 0) {
		/* Need to load a new byte */
		if (strm->avail_in == 0) {
			return -1; /* No more input */
		}

		state->bit_buffer = *strm->next_in++;
		strm->avail_in--;
		strm->total_in++;
		state->bits_in_buffer = 8;
	}

	int bit = state->bit_buffer & 1;
	state->bit_buffer >>= 1;
	state->bits_in_buffer--;
	return bit;
}


/* Get n bits from the input stream (right-aligned) */
static int get_bits(z_stream *strm, inflate_state *state, int n) {
	/* Fast path: if we have enough bits in buffer, use them directly */
	if (state->bits_in_buffer >= n) {
		int result = state->bit_buffer & ((1 << n) - 1);
		state->bit_buffer >>= n;
		state->bits_in_buffer -= n;
		return result;
	}
	
	/* Slow path: need to get more bits from input */
	int result = 0;
	for (int i = 0; i < n; i++) {
		int bit = get_bit(strm, state);
		if (bit < 0) {
			return -1; /* Error - not enough input */
		}
		result |= (bit << i);
	}
	return result;
}


/* Build Huffman tree from code lengths */
static int build_huffman_tree(huffman_table *table, const uint8_t *lengths, int num_codes) {
	/* Count the number of codes for each code length */
	uint16_t bl_count[16] = {0};
	int max_length = 0;
	for (int i = 0; i < num_codes; i++) {
		if (lengths[i] > 15) {
			return Z_DATA_ERROR; /* Invalid code length */
		}
		if (lengths[i] > 0) {
			bl_count[lengths[i]]++;
			max_length = lengths[i] > max_length ? lengths[i] : max_length;
		}
	}

	/* Find the numerical value of the smallest code for each code length */
	uint16_t next_code[16] = {0};
	uint16_t code = 0;
	for (int bits = 1; bits <= 15; bits++) {
		code = (code + bl_count[bits - 1]) << 1;
		next_code[bits] = code;
	}

	/* Assign codes to symbols */
	for (int i = 0; i < num_codes; i++) {
		if (lengths[i] > 0) {
			table->codes[i] = next_code[lengths[i]]++;
			table->lengths[i] = lengths[i];
		} else {
			table->lengths[i] = 0;
		}
	}
	table->count = num_codes;
	return Z_OK;
}

/* Read dynamic Huffman tables */
static int read_dynamic_huffman(z_stream *strm, inflate_state *state) {
	/* Read number of literal/length codes */
	int hlit = get_bits(strm, state, 5);
	if (hlit < 0) return Z_DATA_ERROR;
	hlit += 257;  /* 257-286 codes */

	/* Read number of distance codes */
	int hdist = get_bits(strm, state, 5);
	if (hdist < 0) return Z_DATA_ERROR;
	hdist += 1;  /* 1-32 codes */

	/* Read number of code length codes */
	int hclen = get_bits(strm, state, 4);
	if (hclen < 0) return Z_DATA_ERROR;
	hclen += 4;  /* 4-19 codes */

	/* Read code lengths for code length alphabet */
	uint8_t cl_lengths[19] = {0};
	static const uint8_t cl_order[19] = {
		16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
	};

	for (int i = 0; i < hclen; i++) {
		int len = get_bits(strm, state, 3);
		if (len < 0) return Z_DATA_ERROR;
		cl_lengths[cl_order[i]] = len;
	}

	/* Build code lengths Huffman tree */
	huffman_table cl_table;
	if (build_huffman_tree(&cl_table, cl_lengths, 19) != Z_OK) {
		return Z_DATA_ERROR;
	}

	/* Read literal/length and distance code lengths */
	uint8_t ll_lengths[286 + 32] = {0};  /* Literal/length + distance code lengths */
	int index = 0;
	while (index < hlit + hdist) {
		/* Decode code length */
		int code = 0;
		int len = 0;

		/* Decode Huffman code */
		for (len = 1; len <= 15; len++) {
			int bit = get_bit(strm, state);
			if (bit < 0) return Z_DATA_ERROR;
			code = (code << 1) | bit;

			/* Check if this code matches */
			int found = -1;
			for (int i = 0; i < 19; i++) {
				if (cl_table.lengths[i] == len && cl_table.codes[i] == code) {
					found = i;
					break;
				}
			}
			if (found >= 0) {
				code = found;
				break;
			}
		}

		if (len > 15) return Z_DATA_ERROR;  /* Invalid Huffman code */

		/* Process code */
		if (code < 16) {
			/* Literal code length */
			ll_lengths[index++] = code;
		} else {
			/* Repeat code */
			uint8_t repeat_value;
			int repeat_count;

			switch (code) {
			case 16:  /* Repeat previous code length 3-6 times */
				if (index == 0) return Z_DATA_ERROR;  /* No previous code */
				repeat_value = ll_lengths[index - 1];
				repeat_count = get_bits(strm, state, 2);
				if (repeat_count < 0) return Z_DATA_ERROR;
				repeat_count += 3;
				break;

			case 17:  /* Repeat zero 3-10 times */
				repeat_value = 0;
				repeat_count = get_bits(strm, state, 3);
				if (repeat_count < 0) return Z_DATA_ERROR;
				repeat_count += 3;
				break;

			case 18:  /* Repeat zero 11-138 times */
				repeat_value = 0;
				repeat_count = get_bits(strm, state, 7);
				if (repeat_count < 0) return Z_DATA_ERROR;
				repeat_count += 11;
				break;

			default:
				return Z_DATA_ERROR;  /* Should never happen */
			}

			/* Check for index overflow */
			if (index + repeat_count > hlit + hdist) {
				return Z_DATA_ERROR;
			}

			/* Fill repeated values */
			for (int i = 0; i < repeat_count; i++) {
				ll_lengths[index++] = repeat_value;
			}
		}
	}

	/* Build literal/length Huffman tree */
	if (build_huffman_tree(&state->literals, ll_lengths, hlit) != Z_OK) {
		return Z_DATA_ERROR;
	}

	/* Build distance Huffman tree */
	if (build_huffman_tree(&state->distances, ll_lengths + hlit, hdist) != Z_OK) {
		return Z_DATA_ERROR;
	}

	return Z_OK;
}

/* Read an uncompressed block */
static int read_uncompressed_block(z_stream *strm, inflate_state *state) {
	/* Skip any remaining bits in current byte */
	state->bits_in_buffer = 0;

	/* Read length and negated length */
	if (strm->avail_in < 4) {
		return Z_DATA_ERROR; /* Not enough input */
	}

	uint16_t len = strm->next_in[0] | (strm->next_in[1] << 8);
	uint16_t nlen = strm->next_in[2] | (strm->next_in[3] << 8);

	strm->next_in += 4;
	strm->avail_in -= 4;
	strm->total_in += 4;

	/* Verify length */
	if (len != (uint16_t)~nlen) {
		return Z_DATA_ERROR; /* Length check failed */
	}

	/* Check if we have enough input */
	if (strm->avail_in < len) {
		return Z_DATA_ERROR; /* Not enough input */
	}

	/* Check if we have enough output space */
	if (strm->avail_out < len) {
		return Z_BUF_ERROR; /* Not enough output space */
	}

	/* Copy data directly to output */
	memcpy(strm->next_out, strm->next_in, len);
	strm->next_in += len;
	strm->avail_in -= len;
	strm->total_in += len;
	strm->next_out += len;
	strm->avail_out -= len;
	strm->total_out += len;

	/* Also store in window */
	for (uint16_t i = 0; i < len; i++) {
		state->window[state->window_pos] = strm->next_out[-(len - i)];
		state->window_pos = (state->window_pos + 1) & (state->window_size - 1);
	}

	return Z_OK;
}

/* Initialize fixed Huffman tables */
static void init_fixed_huffman(inflate_state *state) {
	/* Fixed tables as per deflate specification */
	/* Literal/length codes */
	for (int i = 0; i < 144; i++) {
		state->literals.codes[i] = i;
		state->literals.lengths[i] = 8;
	}
	for (int i = 144; i < 256; i++) {
		state->literals.codes[i] = i;
		state->literals.lengths[i] = 9;
	}
	for (int i = 256; i < 280; i++) {
		state->literals.codes[i] = i;
		state->literals.lengths[i] = 7;
	}
	for (int i = 280; i < 288; i++) {
		state->literals.codes[i] = i;
		state->literals.lengths[i] = 8;
	}
	state->literals.count = 288;

	/* Distance codes */
	for (int i = 0; i < 32; i++) {
		state->distances.codes[i] = i;
		state->distances.lengths[i] = 5;
	}
	state->distances.count = 32;
}

/* ----------- Main decoder API functions ----------- */

/* Compatibility wrapper for zlib */
int inflateInit2_(z_stream *strm, int windowBits, const char *version, int stream_size) {
	(void)version;  /* Unused */
	(void)stream_size;  /* Unused */
	return inflateInit2(strm, windowBits);
}

int inflateInit2(z_stream *strm, int windowBits) {
	if (!strm) {
		return Z_STREAM_ERROR;
	}
	/* Handle windowBits - negative means no header */
	int window_size = 1 << ((windowBits < 0) ? -windowBits : windowBits);

	/* Allocate state */
	inflate_state *state = (inflate_state *)calloc(1, sizeof(inflate_state));
	if (!state) {
		return Z_MEM_ERROR;
	}

	/* Allocate window buffer */
	state->window = (uint8_t *)malloc(window_size);
	if (!state->window) {
		free(state);
		return Z_MEM_ERROR;
	}

	/* Initialize state */
	state->window_size = window_size;
	state->window_pos = 0;
	state->bit_buffer = 0;
	state->bits_in_buffer = 0;
	state->final_block = 0;
	state->state = 0; /* Start at block header */

	strm->state = state;
	strm->total_in = 0;
	strm->total_out = 0;

	return Z_OK;
}

int inflate(z_stream *strm, int flush) {
	if (!strm || !strm->state) {
		return Z_STREAM_ERROR;
	}
	inflate_state *state = (inflate_state *)strm->state;
	int ret = Z_OK;

	/* Main decompression loop */
	while (strm->avail_in > 0 && strm->avail_out > 0) {
		/* Process based on current state */
		switch (state->state) {
		case 0: /* Block header */
			/* Read block header */
			state->final_block = get_bit (strm, state);
			if (state->final_block < 0) {
				return Z_DATA_ERROR;
			}
			/* Get block type */
			state->btype = (block_type)get_bits (strm, state, 2);
			if (state->btype < 0) {
				return Z_DATA_ERROR;
			}

			/* Move to block data state */
			state->state = 1;
			break;

		case 1: /* Block data */
			/* Process based on block type */
			switch (state->btype) {
			case BLOCK_UNCOMPRESSED:
				ret = read_uncompressed_block(strm, state);
				if (ret != Z_OK) {
					return ret;
				}
				/* Move to next block header */
				state->state = 0;
				break;

			case BLOCK_FIXED:
				/* Initialize fixed Huffman tables */
				init_fixed_huffman(state);
				/* Go to process literals state */
				state->state = 2;
				break;

			case BLOCK_DYNAMIC:
				/* Read dynamic Huffman tables */
				if (read_dynamic_huffman(strm, state) != Z_OK) {
					return Z_DATA_ERROR;
				}
				/* Go to process literals state */
				state->state = 2;
				break;

			case BLOCK_INVALID:
				return Z_DATA_ERROR;
			}
			break;

		case 2: /* Process literals/lengths */
			/* This is a simplified implementation to handle fixed Huffman codes */
			{
				/* Get a code */
				int code = 0;
				int length = 0;

				/* Decode Huffman code based on current block type */
				if (state->btype == BLOCK_FIXED) {
					/* For fixed Huffman, we know the bit patterns */
					if (get_bit(strm, state) == 0) {
						/* 0xxx xxxx - 8 bit code for values 0-143 */
						code = get_bits (strm, state, 7);
						if (code < 0) {
							return Z_DATA_ERROR;
						}
						code = (0 << 7) | code; /* Add the 0 bit we already read */
					} else {
						int next_bit = get_bit (strm, state);
						if (next_bit < 0) {
							return Z_DATA_ERROR;
						}
						if (next_bit == 0) {
							/* 10xx xxxx - 8 bit code for values 144-255 */
							code = get_bits (strm, state, 6);
							if (code < 0) {
								return Z_DATA_ERROR;
							}
							code = (0b10 << 6) | code; /* Add the 10 bits we already read */
							code += 144; /* Adjust to actual value */
						} else {
							int third_bit = get_bit(strm, state);
							if (third_bit < 0) {
								return Z_DATA_ERROR;
							}
							
							if (third_bit == 0) {
								/* 110x xxxx - 7 bit code for values 256-279 */
								code = get_bits (strm, state, 4);
								if (code < 0) {
									return Z_DATA_ERROR;
								}
								code = (0b110 << 4) | code; /* Add the 110 bits we already read */
								code = code - (0b110 << 4) + 256; /* Adjust to actual value */
							} else {
								/* 111x xxxx - 8 bit code for values 280-287 */
								code = get_bits (strm, state, 4);
								if (code < 0) {
									return Z_DATA_ERROR;
								}
								code = (0b111 << 4) | code; /* Add the 111 bits we already read */
								code = code - (0b111 << 4) + 280; /* Adjust to actual value */
							}
						}
					}
				} else if (state->btype == BLOCK_DYNAMIC) {
					/* For dynamic Huffman, use the constructed tables */
					code = 0;
					for (length = 1; length <= 15; length++) {
						int bit = get_bit(strm, state);
						if (bit < 0) {
							return Z_DATA_ERROR;
						}
						code = (code << 1) | bit;
						
						/* Look for matching code */
						int found = -1;
						for (int i = 0; i < state->literals.count; i++) {
							if (state->literals.lengths[i] == length && 
								state->literals.codes[i] == code) {
								found = i;
								break;
							}
						}
						if (found >= 0) {
							code = found;
							break;
						}
					}
					
					if (length > 15) {
						return Z_DATA_ERROR; /* Invalid Huffman code */
					}
				} else {
					return Z_DATA_ERROR; /* Invalid block type */
				}

				/* Process the code */
				if (code < 256) {
					/* Literal byte */
					*strm->next_out++ = (uint8_t)code;
					strm->avail_out--;
					strm->total_out++;

					/* Add to window */
					state->window[state->window_pos] = (uint8_t)code;
					state->window_pos = (state->window_pos + 1) & (state->window_size - 1);
				} else if (code == 256) {
					/* End of block */
					state->state = 0; /* Back to block header */
					if (state->final_block) {
						return Z_STREAM_END; /* We're done */
					}
				} else if (code <= 285) {
					/* Length code */
					/* Determine base length and extra bits */
					static const uint16_t length_base[] = {
						3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
						15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
						67, 83, 99, 115, 131, 163, 195, 227, 258
					};
					static const uint8_t length_extra[] = {
						0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
						1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
						4, 4, 4, 4, 5, 5, 5, 5, 0
					};

					int length_idx = code - 257;
					int length = length_base[length_idx];
					int extra_bits = length_extra[length_idx];

					if (extra_bits > 0) {
						int extra = get_bits (strm, state, extra_bits);
						if (extra < 0) {
							return Z_DATA_ERROR;
						}
						length += extra;
					}

					/* Now read distance code */
					int distance_code;
					if (state->btype == BLOCK_FIXED) {
						/* For fixed Huffman, distance code is always 5 bits */
						distance_code = get_bits (strm, state, 5);
						if (distance_code < 0) {
							return Z_DATA_ERROR;
						}
					} else if (state->btype == BLOCK_DYNAMIC) {
						/* For dynamic Huffman, use the distance table */
						code = 0;
						int len;
						for (len = 1; len <= 15; len++) {
							int bit = get_bit(strm, state);
							if (bit < 0) {
								return Z_DATA_ERROR;
							}
							code = (code << 1) | bit;
							
							/* Look for matching distance code */
							int found = -1;
							for (int i = 0; i < state->distances.count; i++) {
								if (state->distances.lengths[i] == len && 
									state->distances.codes[i] == code) {
									found = i;
									break;
								}
							}
							if (found >= 0) {
								distance_code = found;
								break;
							}
						}
						if (len > 15) {
							return Z_DATA_ERROR; /* Invalid Huffman code */
						}
					} else {
						return Z_DATA_ERROR; /* Invalid block type */
					}
					/* Convert to actual distance */
					static const uint16_t dist_base[] = {
						1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
						33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
						1025, 1537, 2049, 3073, 4097, 6145,
						8193, 12289, 16385, 24577
					};
					static const uint8_t dist_extra[] = {
						0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
						4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
						9, 9, 10, 10, 11, 11,
						12, 12, 13, 13
					};

					int distance = dist_base[distance_code];
					int dist_extra_bits = dist_extra[distance_code];

					if (dist_extra_bits > 0) {
						int extra = get_bits (strm, state, dist_extra_bits);
						if (extra < 0) {
							return Z_DATA_ERROR;
						}
						distance += extra;
					}

					/* Copy bytes from window */
					if (distance > state->window_size) {
						return Z_DATA_ERROR; /* Distance too far back */
					}

					/* Make sure we have enough output space */
					if (strm->avail_out < length) {
						return Z_BUF_ERROR;
					}

					/* Copy the bytes */
					for (int i = 0; i < length; i++) {
						uint8_t byte = state->window[(state->window_pos - distance) & (state->window_size - 1)];
						*strm->next_out++ = byte;
						strm->avail_out--;
						strm->total_out++;

						/* Add to window */
						state->window[state->window_pos] = byte;
						state->window_pos = (state->window_pos + 1) & (state->window_size - 1);
					}
				} else {
					/* Invalid code */
					return Z_DATA_ERROR;
				}
			}
			break;

		default:
			return Z_STREAM_ERROR; /* Invalid state */
		}

		/* Check if we're done */
		if (state->final_block && state->state == 0) {
			return Z_STREAM_END;
		}

		/* Check if we should stop */
		if (flush == Z_FINISH && strm->avail_in == 0) {
			if (!state->final_block || state->state != 0) {
				return Z_BUF_ERROR; /* Need more input to finish */
			}
			return Z_STREAM_END;
		}
	}

	/* If we get here, we've run out of input or output space */
	if (strm->avail_out == 0) {
		return Z_BUF_ERROR;
	}

	return ret;
}

int inflateEnd(z_stream *strm) {
	if (!strm || !strm->state) {
		return Z_STREAM_ERROR;
	}
	inflate_state *state = (inflate_state *)strm->state;
	free (state->window);
	free (state);
	strm->state = NULL;

	return Z_OK;
}
