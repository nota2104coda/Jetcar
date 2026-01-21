#include "MavlinkComm.h"
#include <Arduino.h>
#include <string.h>

// MAVLink v2 minimal implementation (framing + CRC). This is a small, reviewed
// implementation meant to be used in resource constrained embedded code until
// you switch to generated MAVLink headers via mavgen.

static constexpr uint8_t MAVLINK_STX = 0xFD; // MAVLink v2 STX
static constexpr uint8_t MAVLINK_SYSID = 1;
static constexpr uint8_t MAVLINK_COMPID = 1;
static constexpr uint32_t MAVLINK_MSG_ID_TELEMETRY = 200;
static constexpr uint32_t MAVLINK_MSG_ID_MOTOR_CMD = 201;
static constexpr uint8_t MAVLINK_CRC_EXTRA_TELEMETRY = 50;
static constexpr uint8_t MAVLINK_CRC_EXTRA_MOTOR_CMD = 51;
static constexpr size_t UART_BUFSIZE = 256;

// Local snapshot of telemetry (volatile to be safe when updated in critical sections)
static volatile JetsonTelemetryPacket g_latestTelemetry = {};
static MotorCommandCallback g_motorCb = nullptr;

// Internal tx sequence
static uint8_t g_tx_seq = 0;

// UART RX buffer
static uint8_t g_rxbuf[UART_BUFSIZE];
static size_t g_rxbuf_len = 0;

// ---------------------------------------------------------------------------
// CRC (X.25 / CRC16/MCRF4XX) helpers from MAVLink source (small, portable)
static inline void crc_accumulate(uint8_t data, uint16_t *crcAccum) {
  uint8_t tmp;
  tmp = data ^ (uint8_t)(*crcAccum & 0xff);
  tmp ^= (tmp << 4);
  *crcAccum = (*crcAccum >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4);
}
static inline void crc_init(uint16_t* crcAccum) {
  *crcAccum = 0xFFFF;
}
static inline uint16_t crc_calculate(const uint8_t* pBuffer, uint16_t length) {
  uint16_t crcTmp;
  crc_init(&crcTmp);
  while (length--) {
    crc_accumulate(*pBuffer++, &crcTmp);
  }
  return crcTmp;
}

// ---------------------------------------------------------------------------
// Little-endian writers (MISRA-friendly)
static inline void write_u32_le(uint8_t *pkt, size_t &off, uint32_t v) {
  pkt[off++] = (uint8_t)(v & 0xFFu);
  pkt[off++] = (uint8_t)((v >> 8) & 0xFFu);
  pkt[off++] = (uint8_t)((v >> 16) & 0xFFu);
  pkt[off++] = (uint8_t)((v >> 24) & 0xFFu);
}
static inline void write_i16_le(uint8_t *pkt, size_t &off, int16_t v) {
  pkt[off++] = (uint8_t)(v & 0xFF);
  pkt[off++] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void write_u8(uint8_t *pkt, size_t &off, uint8_t v) {
  pkt[off++] = v;
}
static inline void write_f32_le(uint8_t *pkt, size_t &off, float f) {
  static_assert(sizeof(float) == sizeof(uint32_t), "float size unexpected");
  uint32_t tmp = 0u;
  memcpy(&tmp, &f, sizeof(tmp));
  write_u32_le(pkt, off, tmp);
}

// ---------------------------------------------------------------------------
// Serialize the telemetry payload into the provided buffer and return length
static uint8_t serializeTelemetryPayload(uint8_t *buf) {
  size_t off = 0;
  // Snapshot the volatile telemetry into a local copy first
  JetsonTelemetryPacket local;
  noInterrupts();
  local = g_latestTelemetry;
  interrupts();

  write_u32_le(buf, off, local.timestamp_ms);
  write_f32_le(buf, off, local.speedFL);
  write_f32_le(buf, off, local.speedFR);
  write_f32_le(buf, off, local.speedRL);
  write_f32_le(buf, off, local.speedRR);
  write_i16_le(buf, off, local.sonarFrontcm);
  write_i16_le(buf, off, local.sonarRearcm);
  write_u8(buf, off, local.cliffFront);
  write_u8(buf, off, local.cliffRear);
  return (uint8_t)off;
}

// Pack a MAVLink v2 message into out_buf; returns number of bytes written
static uint16_t packMavlinkV2(uint32_t msgid, const uint8_t* payload, uint8_t payload_len, uint8_t seq, uint8_t sysid, uint8_t compid, uint8_t crc_extra, uint8_t* out_buf) {
  size_t off = 0;
  out_buf[off++] = MAVLINK_STX;
  out_buf[off++] = payload_len;
  out_buf[off++] = 0; // incompat_flags
  out_buf[off++] = 0; // compat_flags
  out_buf[off++] = seq;
  out_buf[off++] = sysid;
  out_buf[off++] = compid;
  out_buf[off++] = (uint8_t)(msgid & 0xFF);
  out_buf[off++] = (uint8_t)((msgid >> 8) & 0xFF);
  out_buf[off++] = (uint8_t)((msgid >> 16) & 0xFF);

  if (payload_len) {
    memcpy(out_buf + off, payload, payload_len);
    off += payload_len;
  }

  // CRC over LEN..MSGID(3) + payload
  uint16_t crc = crc_calculate(&out_buf[1], (uint16_t)(9 + payload_len));
  crc_accumulate(crc_extra, &crc);
  out_buf[off++] = (uint8_t)(crc & 0xFF);
  out_buf[off++] = (uint8_t)((crc >> 8) & 0xFF);
  return (uint16_t)off;
}

// If generated MAVLink headers are available, prefer them for clean packing/parsing.
#ifdef MAVLINK_MSG_ID_PICOW_TELEMETRY
#include <mavlink.h>
#endif

// Called from main loop - read Serial1 and parse frames (prefer generated mavlink when available)
void mavlinkHandleUART() {
#ifdef MAVLINK_MSG_ID_PICOW_TELEMETRY
  // Use generated MAVLink parser (clean and robust)
  static mavlink_message_t msg;
  static mavlink_status_t status;
  while (Serial1.available() > 0) {
    int c = Serial1.read();
    if (c < 0) break;
    if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t)c, &msg, &status)) {
      if (msg.msgid == MAVLINK_MSG_ID_PICOW_MOTOR_CMD) {
        float fr = mavlink_msg_picow_motor_cmd_get_tqFR(&msg);
        float fl = mavlink_msg_picow_motor_cmd_get_tqFL(&msg);
        float rr = mavlink_msg_picow_motor_cmd_get_tqRR(&msg);
        float rl = mavlink_msg_picow_motor_cmd_get_tqRL(&msg);
        if (g_motorCb) g_motorCb(fr, fl, rr, rl);
      }
    }
  }
#else
  // Fallback to manual parser
  while ((Serial1.available() > 0) && (g_rxbuf_len < UART_BUFSIZE)) {
    int b = Serial1.read();
    if (b < 0) break;
    g_rxbuf[g_rxbuf_len++] = (uint8_t)b;
  }
  if (g_rxbuf_len == 0) return;

  size_t idx = 0;
  while (idx + 10 <= g_rxbuf_len) {
    if (g_rxbuf[idx] != MAVLINK_STX) { idx++; continue; }
    if (idx + 10 > g_rxbuf_len) break;
    uint8_t plen = g_rxbuf[idx + 1];
    size_t frame_len = 10 + plen + 2;
    if (idx + frame_len > g_rxbuf_len) break;

    uint16_t crc = crc_calculate(&g_rxbuf[idx + 1], (uint16_t)(9 + plen));
    uint32_t r_msgid = (uint32_t)g_rxbuf[idx + 7] | ((uint32_t)g_rxbuf[idx + 8] << 8) | ((uint32_t)g_rxbuf[idx + 9] << 16);
    uint8_t crc_extra = 0;
    if (r_msgid == MAVLINK_MSG_ID_MOTOR_CMD) crc_extra = MAVLINK_CRC_EXTRA_MOTOR_CMD;
    else if (r_msgid == MAVLINK_MSG_ID_TELEMETRY) crc_extra = MAVLINK_CRC_EXTRA_TELEMETRY;
    if (crc_extra) crc_accumulate(crc_extra, &crc);

    uint8_t ck0 = g_rxbuf[idx + 10 + plen];
    uint8_t ck1 = g_rxbuf[idx + 10 + plen + 1];
    if ((uint8_t)(crc & 0xFF) == ck0 && (uint8_t)((crc >> 8) & 0xFF) == ck1) {
      // Valid frame
      if (r_msgid == MAVLINK_MSG_ID_MOTOR_CMD && plen >= 16) {
        size_t poff = idx + 10;
        float fr, fl, rr, rl;
        uint32_t tmp;
        memcpy(&tmp, &g_rxbuf[poff], 4); poff += 4; memcpy(&fr, &tmp, 4);
        memcpy(&tmp, &g_rxbuf[poff], 4); poff += 4; memcpy(&fl, &tmp, 4);
        memcpy(&tmp, &g_rxbuf[poff], 4); poff += 4; memcpy(&rr, &tmp, 4);
        memcpy(&tmp, &g_rxbuf[poff], 4); poff += 4; memcpy(&rl, &tmp, 4);
        if (g_motorCb) {
          g_motorCb(fr, fl, rr, rl);
        }
      }
      idx += frame_len;
    } else {
      idx++;
    }
  }

  // compact remaining
  if (idx < g_rxbuf_len) {
    memmove(g_rxbuf, g_rxbuf + idx, g_rxbuf_len - idx);
    g_rxbuf_len -= idx;
  } else {
    g_rxbuf_len = 0;
  }
#endif
}

void mavlinkSendTelemetry() {
#ifdef MAVLINK_MSG_ID_PICOW_TELEMETRY
  // Use generated pack
  mavlink_message_t msg;
  mavlink_msg_picow_telemetry_pack(MAVLINK_SYSID, MAVLINK_COMPID, &msg,
    g_latestTelemetry.timestamp_ms,
    g_latestTelemetry.speedFL,
    g_latestTelemetry.speedFR,
    g_latestTelemetry.speedRL,
    g_latestTelemetry.speedRR,
    g_latestTelemetry.sonarFrontcm,
    g_latestTelemetry.sonarRearcm,
    g_latestTelemetry.cliffFront,
    g_latestTelemetry.cliffRear,
    g_latestTelemetry.accelX,
    g_latestTelemetry.accelY,
    g_latestTelemetry.gyroZ,
    g_latestTelemetry.tempC);
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  Serial1.write(buf, len);
#else
  uint8_t payload[64];
  uint8_t plen = serializeTelemetryPayload(payload);
  uint8_t msgbuf[10 + 64 + 2];
  uint16_t len = packMavlinkV2(MAVLINK_MSG_ID_TELEMETRY, payload, plen, g_tx_seq++, MAVLINK_SYSID, MAVLINK_COMPID, MAVLINK_CRC_EXTRA_TELEMETRY, msgbuf);
  Serial1.write(msgbuf, len);
#endif
}

void mavlinkSendMotorCmd(float fr, float fl, float rr, float rl) {
#ifdef MAVLINK_MSG_ID_PICOW_MOTOR_CMD
  mavlink_message_t msg;
  mavlink_msg_picow_motor_cmd_pack(MAVLINK_SYSID, MAVLINK_COMPID, &msg, fr, fl, rr, rl);
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  Serial1.write(buf, len);
#else
  uint8_t payload[16];
  size_t off = 0;
  memcpy(payload + off, &fr, 4); off += 4;
  memcpy(payload + off, &fl, 4); off += 4;
  memcpy(payload + off, &rr, 4); off += 4;
  memcpy(payload + off, &rl, 4); off += 4;
  uint8_t msgbuf[10 + 16 + 2];
  uint16_t len = packMavlinkV2(MAVLINK_MSG_ID_MOTOR_CMD, payload, 16, g_tx_seq++, MAVLINK_SYSID, MAVLINK_COMPID, MAVLINK_CRC_EXTRA_MOTOR_CMD, msgbuf);
  Serial1.write(msgbuf, len);
#endif
}
void mavlinkUpdateTelemetry(const JetsonTelemetryPacket &t) {
  noInterrupts();
  g_latestTelemetry = t;
  interrupts();
}

void mavlinkSetMotorCallback(MotorCommandCallback cb) {
  g_motorCb = cb;
}

void mavlinkInit() {
  // Placeholder: nothing to init right now but provides an entrypoint for future work.
}
