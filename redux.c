/*
 * Copyright (C) 2025 Peter Lawrence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include "parameters.h"
#include "urandom.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

static const int tcp_server_port = 51111;

static const uint8_t sdp_request[] =
{
  0x01, /* V2GTP Version 1 */
  0xfe, /* inverted protocol */
  0x90, 0x00, /* SDP REQUEST */
  0x00, 0x00, 0x00, 0x02, /* payload length */
  0x10, /* TLS: no */
  0x00, /* TCP protocol */
};

static uint8_t sdp_response[28] =
{
  0x01, /* V2GTP Version 1 */
  0xfe, /* inverted protocol */
  0x90, 0x01, /* SDP RESPONSE */
  0x00, 0x00, 0x00, 0x14, /* payload length */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* TBD IPv6 address */
  (uint8_t)(tcp_server_port >> 8), (uint8_t)(tcp_server_port >> 0),
  0x10, /* TLS: no */
  0x00, /* TCP protocol */
};

static struct in6_addr *localip = NULL;

static struct iso1PhysicalValueType EVTargetVoltage, EVTargetCurrent;

static void get_link_local_addr(const char *ifname)
{
  struct ifaddrs *ifaddr, *ifa;
  static struct in6_addr foundip;

  if (getifaddrs(&ifaddr) == -1) return;

  for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr->sa_family != AF_INET6) continue;

    if (strncmp(ifa->ifa_name, ifname, sizeof(ifname))) continue;

    struct sockaddr_in6 *current_addr = (struct sockaddr_in6 *) ifa->ifa_addr;

    if (!IN6_IS_ADDR_LINKLOCAL(&(current_addr->sin6_addr))) continue;

    memcpy(&foundip, &current_addr->sin6_addr, sizeof(current_addr->sin6_addr));
    localip = &foundip;
  }

  freeifaddrs(ifaddr);
}

/* utility function to configure a socket as non-blocking */

static void set_nonblocking(int sock)
{
  int opts = fcntl(sock, F_GETFL);
  if (opts <= 0) return;
  opts = (opts | O_NONBLOCK);
  fcntl(sock, F_SETFL, opts);
}

/* utility function to FD_SET and track highest socket */

static int set_reads(fd_set *reads, int sock, int highest_socket)
{
  FD_SET(sock, reads);
  if (sock > highest_socket) highest_socket = sock;
  return highest_socket;
}

/* helper to translate string into OpenV2G format */

static uint16_t set_chars(exi_string_character_t *chars, uint16_t maxlen, char *string)
{
  uint16_t len = 0;
  while (*string && (len < maxlen))
  {
    *chars++ = *string++;
    len++;
  }
  return len;
}

int main(int argc, char *argv[])
{
  int rc;
  uint8_t buffer[4096];
  struct sockaddr_in6 server_addr;
  const char *ifname = (argc > 1) ? argv[1] : "seth0";
  bool handshake_expected;

  get_link_local_addr(ifname);

  if (!localip)
  {
    fprintf(stderr, "ERROR: interface (%s) not found\n", ifname);
    return -1;
  }

  urandom_init();

  /*
  setup SDP server
  */

  int sdp_sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (sdp_sock < 0) return -1;

  struct ipv6_mreq mreq;
  inet_pton(AF_INET6, "ff02::1", &mreq.ipv6mr_multiaddr);
  mreq.ipv6mr_interface = if_nametoindex(ifname);

  rc = setsockopt(sdp_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *) &mreq, sizeof(mreq));

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_addr = in6addr_any;
  server_addr.sin6_port = htons(15118);
  server_addr.sin6_scope_id = if_nametoindex(ifname);

  memcpy(sdp_response + 8, &localip->s6_addr, sizeof(localip->s6_addr));

  rc = bind(sdp_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr));
  if (rc < 0)
  {
    fprintf(stderr, "SDP bind error %d\n", rc);
    return -1;
  }

  set_nonblocking(sdp_sock);

  /*
  setup ISO server
  */

  int peer_sock = -1, listen_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  if (listen_sock < 0) return -1;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_addr = in6addr_any;
  server_addr.sin6_port = htons(tcp_server_port);
  server_addr.sin6_scope_id = if_nametoindex(ifname);

  rc = bind(listen_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr));
  if (rc < 0)
  {
    fprintf(stderr, "ISO bind error %d\n", rc);
    return -1;
  }

  set_nonblocking(listen_sock);

  rc = listen(listen_sock, 1);

  for (;;)
  {
    fd_set reads, writes;
    FD_ZERO(&reads);
    int highest_socket = -1;
    highest_socket = set_reads(&reads, sdp_sock, highest_socket);
    highest_socket = set_reads(&reads, listen_sock, highest_socket);
    if (peer_sock > 0)
      highest_socket = set_reads(&reads, peer_sock, highest_socket);
    FD_ZERO(&writes);

    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
    rc = select(highest_socket + 1, &reads, &writes, NULL, &tv);

    if (rc < 0) break;
    if (0 == rc) continue;

    if (FD_ISSET(sdp_sock, &reads))
    {
      struct sockaddr_in6 client_addr;
      socklen_t client_len = sizeof(client_addr);
      ssize_t len = recvfrom(sdp_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);

      printf("recv %d\n", len);
      if (len == sizeof(sdp_request))
        if (0 == memcmp(buffer, sdp_request, sizeof(sdp_request)))
        {
          rc = sendto(sdp_sock, sdp_response, sizeof(sdp_response), 0, (struct sockaddr *)&client_addr, client_len);
          printf("xmit %d\n", rc);
        }
     }

    if (FD_ISSET(listen_sock, &reads))
    {
      peer_sock = accept(listen_sock, NULL, NULL);
      handshake_expected = true;
    }

    if (FD_ISSET(peer_sock, &reads))
    {
      ssize_t len = recv(peer_sock, buffer, sizeof(buffer), 0);
      printf("recv %d\n", len);
      if (len <= 0)
      {
        close(peer_sock);
        peer_sock = -1;
        rc = listen(listen_sock, 1);
      }
      else
      {
        bitstream_t streamIn, streamOut;
        size_t posi = 0, poso = 0;
        int errn;
        uint32_t payloadLength;

        streamIn.size = len;
        streamIn.data = (uint8_t *)buffer;
        streamIn.pos = &posi;

        streamOut.size = sizeof(buffer);
        streamOut.data = buffer;
        streamOut.pos = &poso;

        errn = read_v2gtpHeader(streamIn.data, &payloadLength);
      printf("read %d\n", errn);
        if (errn) continue;

        *streamIn.pos += V2GTP_HEADER_LENGTH;

        if (handshake_expected)
        {
          struct appHandEXIDocument exiDoc, appHandResp;

          errn = decode_appHandExiDocument(&streamIn, &exiDoc);
          printf("hand %d\n", errn);
          if (errn) continue;

          int selectedSchema = -1;
          for(int i=0;i<exiDoc.supportedAppProtocolReq.AppProtocol.arrayLen;i++) {
            if (exiDoc.supportedAppProtocolReq.AppProtocol.array[i].ProtocolNamespace.charactersLen == (ARRAY_SIZE(iso1string) - 1))
            {
              int j;
              for (j = 0; j < (ARRAY_SIZE(iso1string) - 1); j++)
                if (exiDoc.supportedAppProtocolReq.AppProtocol.array[i].ProtocolNamespace.characters[j] != iso1string[j]) break;
              if ((ARRAY_SIZE(iso1string) - 1) == j) selectedSchema = exiDoc.supportedAppProtocolReq.AppProtocol.array[i].SchemaID;
            }
          }

          printf("selectedSchema %d\n", selectedSchema);
          if (-1 == selectedSchema) continue;

          init_appHandEXIDocument(&appHandResp);
          appHandResp.supportedAppProtocolRes_isUsed = 1u;
          appHandResp.supportedAppProtocolRes.ResponseCode = appHandresponseCodeType_OK_SuccessfulNegotiation;
          /* signal the protocol by the provided schema id */
          appHandResp.supportedAppProtocolRes.SchemaID = selectedSchema;
          appHandResp.supportedAppProtocolRes.SchemaID_isUsed = 1u;

          *streamOut.pos = V2GTP_HEADER_LENGTH;
          errn = encode_appHandExiDocument(&streamOut, &appHandResp);
          printf("handenc %d\n", errn);
          if (errn) continue;
          errn = write_v2gtpHeader(streamOut.data, *streamOut.pos-V2GTP_HEADER_LENGTH, V2GTP_EXI_TYPE);
          printf("handwrite %d %d\n", errn, (int)*streamOut.pos);

          handshake_expected = false;
        }
        else
        {
          struct iso1EXIDocument exiIn, exiOut;

          /* OpenV2G init_ helpers don't clear everything; trust only memset */
          memset(&exiOut, 0, sizeof(exiOut)); 
          memset(&exiIn, 0, sizeof(exiIn));

          decode_iso1ExiDocument(&streamIn, &exiIn);
          printf("iso %d %d %d\n", errn, exiIn.V2G_Message_isUsed, exiIn.V2G_Message.Body.SessionSetupReq_isUsed);
          if (!exiIn.V2G_Message_isUsed) continue;
          exiOut.V2G_Message_isUsed = 1u;

          /*
          NOTE: the following V2G message types are deliberately not handled:
          ServiceDetailReq (opt VAS)
          MeteringReceiptReq (opt Metering)
          CertificateUpdateReq (opt Certificate Installation)
          CertificateInstallationReq (opt Certificate Update)
          ChargingStatusReq (specific to AC charging) 
          */

          if (exiIn.V2G_Message.Body.SessionSetupReq_isUsed) {

            exiOut.V2G_Message.Header.SessionID.bytesLen = 8;
            urandom_get(exiOut.V2G_Message.Header.SessionID.bytes, exiOut.V2G_Message.Header.SessionID.bytesLen);
            exiOut.V2G_Message.Body.SessionSetupRes_isUsed = 1u;
            struct iso1SessionSetupResType *body = &exiOut.V2G_Message.Body.SessionSetupRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->EVSEID.charactersLen = set_chars(body->EVSEID.characters, ARRAY_SIZE(body->EVSEID.characters), "ZZ00000");
            body->EVSETimeStamp_isUsed = 0u;

          } else if (exiIn.V2G_Message.Body.ServiceDiscoveryReq_isUsed) {

            exiOut.V2G_Message.Body.ServiceDiscoveryRes_isUsed = 1u;
            struct iso1ServiceDiscoveryResType *body = &exiOut.V2G_Message.Body.ServiceDiscoveryRes;
            body->ServiceList_isUsed = 0u;
            body->ResponseCode = iso1responseCodeType_OK;
            body->PaymentOptionList.PaymentOption.array[0] = iso1paymentOptionType_ExternalPayment;
            body->PaymentOptionList.PaymentOption.arrayLen = 1;
            body->ChargeService.ServiceCategory = iso1serviceCategoryType_EVCharging;
            body->ChargeService.FreeService = 1;
            body->ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.arrayLen = 1;
            body->ChargeService.SupportedEnergyTransferMode.EnergyTransferMode.array[0] = dcmode;

          } else if (exiIn.V2G_Message.Body.PaymentServiceSelectionReq_isUsed) {

            exiOut.V2G_Message.Body.PaymentServiceSelectionRes_isUsed = 1u;
            exiOut.V2G_Message.Body.ServiceDetailRes.ResponseCode = iso1responseCodeType_OK;

          } else if (exiIn.V2G_Message.Body.PaymentDetailsReq_isUsed) {

            exiOut.V2G_Message.Body.PaymentDetailsRes_isUsed = 1u;
            exiOut.V2G_Message.Body.PaymentDetailsRes.ResponseCode = iso1responseCodeType_OK;

          } else if (exiIn.V2G_Message.Body.AuthorizationReq_isUsed) {

            exiOut.V2G_Message.Body.AuthorizationRes_isUsed = 1u;
            exiOut.V2G_Message.Body.AuthorizationRes.ResponseCode = iso1responseCodeType_OK;
            exiOut.V2G_Message.Body.AuthorizationRes.EVSEProcessing = iso1EVSEProcessingType_Finished;

          } else if (exiIn.V2G_Message.Body.ChargeParameterDiscoveryReq_isUsed) {

            exiOut.V2G_Message.Body.ChargeParameterDiscoveryRes_isUsed = 1u;
            bool compatible = (dcmode == exiIn.V2G_Message.Body.ChargeParameterDiscoveryReq.RequestedEnergyTransferMode);
            struct iso1ChargeParameterDiscoveryResType *body = &exiOut.V2G_Message.Body.ChargeParameterDiscoveryRes;
            body->ResponseCode = (compatible) ? iso1responseCodeType_OK : iso1responseCodeType_FAILED_WrongEnergyTransferMode;
            body->EVSEProcessing = iso1EVSEProcessingType_Finished;
            body->DC_EVSEChargeParameter_isUsed = 1;
            body->DC_EVSEChargeParameter = dccharge;

          } else if (exiIn.V2G_Message.Body.PowerDeliveryReq_isUsed) {

            exiOut.V2G_Message.Body.PowerDeliveryRes_isUsed = 1u;
            struct iso1PowerDeliveryResType *body = &exiOut.V2G_Message.Body.PowerDeliveryRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->EVSEStatus_isUsed = 1;
            body->EVSEStatus.EVSENotification = iso1EVSENotificationType_StopCharging;
            body->EVSEStatus.NotificationMaxDelay = max_delay;

          } else if (exiIn.V2G_Message.Body.SessionStopReq_isUsed) {

            exiOut.V2G_Message.Body.SessionStopRes_isUsed = 1u;
            exiOut.V2G_Message.Body.SessionStopRes.ResponseCode = iso1responseCodeType_OK;

          } else if (exiIn.V2G_Message.Body.CableCheckReq_isUsed) {

            exiOut.V2G_Message.Body.CableCheckRes_isUsed = 1u;
            struct iso1CableCheckResType *body = &exiOut.V2G_Message.Body.CableCheckRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->DC_EVSEStatus.NotificationMaxDelay = max_delay;
            body->DC_EVSEStatus.EVSENotification = iso1EVSENotificationType_None;
            body->EVSEProcessing = iso1EVSEProcessingType_Finished;

          } else if (exiIn.V2G_Message.Body.PreChargeReq_isUsed) {

            /* jot down the targets */
            EVTargetVoltage = exiIn.V2G_Message.Body.PreChargeReq.EVTargetVoltage;
            EVTargetCurrent = exiIn.V2G_Message.Body.PreChargeReq.EVTargetCurrent;

            exiOut.V2G_Message.Body.PreChargeRes_isUsed = 1u;
            struct iso1PreChargeResType *body = &exiOut.V2G_Message.Body.PreChargeRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->DC_EVSEStatus.EVSENotification = iso1EVSENotificationType_None;
            body->DC_EVSEStatus.NotificationMaxDelay = max_delay;
            body->EVSEPresentVoltage = EVTargetVoltage;

          } else if (exiIn.V2G_Message.Body.PowerDeliveryReq_isUsed) {

            exiOut.V2G_Message.Body.PowerDeliveryRes_isUsed = 1u;
            struct iso1PowerDeliveryResType *body = &exiOut.V2G_Message.Body.PowerDeliveryRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->DC_EVSEStatus_isUsed = 1;
            body->DC_EVSEStatus.EVSENotification = iso1EVSENotificationType_None;
            body->DC_EVSEStatus.NotificationMaxDelay = max_delay;

          } else if (exiIn.V2G_Message.Body.WeldingDetectionReq_isUsed) {

            exiOut.V2G_Message.Body.WeldingDetectionRes_isUsed = 1u;
            struct iso1WeldingDetectionResType *body = &exiOut.V2G_Message.Body.WeldingDetectionRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->DC_EVSEStatus.EVSENotification = iso1EVSENotificationType_None;
            body->DC_EVSEStatus.NotificationMaxDelay = max_delay;
            body->EVSEPresentVoltage = EVTargetVoltage;

          } else if (exiIn.V2G_Message.Body.CurrentDemandReq_isUsed) {

            /* jot down the targets */
            EVTargetVoltage = exiIn.V2G_Message.Body.CurrentDemandReq.EVTargetVoltage;
            EVTargetCurrent = exiIn.V2G_Message.Body.CurrentDemandReq.EVTargetCurrent;

            exiOut.V2G_Message.Body.CurrentDemandRes_isUsed = 1u;
            struct iso1CurrentDemandResType *body = &exiOut.V2G_Message.Body.CurrentDemandRes;
            body->ResponseCode = iso1responseCodeType_OK;
            body->DC_EVSEStatus.NotificationMaxDelay = max_delay;
            body->DC_EVSEStatus.EVSENotification = iso1EVSENotificationType_None;
            body->DC_EVSEStatus.EVSEStatusCode = iso1DC_EVSEStatusCodeType_EVSE_Ready;
            body->EVSEPresentVoltage = EVTargetVoltage;
            body->EVSEPresentCurrent = EVTargetCurrent;

          } else {

            printf("unhandled\n");
            continue;

          }

          /* if the value has not already been written, echo back the provided SessionID (if any) */
          if (!exiOut.V2G_Message.Header.SessionID.bytesLen && exiIn.V2G_Message.Header.SessionID.bytesLen)
          {
            exiOut.V2G_Message.Header.SessionID.bytesLen = exiIn.V2G_Message.Header.SessionID.bytesLen;
            memcpy(exiOut.V2G_Message.Header.SessionID.bytes, exiIn.V2G_Message.Header.SessionID.bytes, exiOut.V2G_Message.Header.SessionID.bytesLen);
          }

          *streamOut.pos = V2GTP_HEADER_LENGTH;
          errn = encode_iso1ExiDocument(&streamOut, &exiOut);
          printf("isoenc %d\n", errn);
          if (errn) continue;
          errn = write_v2gtpHeader(streamOut.data, *streamOut.pos-V2GTP_HEADER_LENGTH, V2GTP_EXI_TYPE);
          printf("isowrite %d %d\n", errn, (int)*streamOut.pos);
        }

	      send(peer_sock, streamOut.data, *streamOut.pos, 0);
      }
    }
  }

  close(sdp_sock);
  close(listen_sock);
  if (peer_sock > 0) close(peer_sock);
  urandom_deinit();

  return 0;
}

