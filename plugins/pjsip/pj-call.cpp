/*
 * This file is part of switcher-pjsip.
 *
 * switcher-myplugin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "pj-call.h"
#include "pj-sip.h"
#include <glib/gstdio.h> //g_remove

namespace switcher
{

  PJCall::app_t PJCall::app;
  pjmedia_endpt *PJCall::med_endpt_ = NULL;

  /* Codec constants */
  PJCall::codec_t PJCall::audio_codecs[] = 
    {
      { 0,  "PCMU", 8000, 64000, 20, "G.711 ULaw" },
      { 3,  "GSM",  8000, 13200, 20, "GSM" },
      { 4,  "G723", 8000, 6400,  30, "G.723.1" },
      { 8,  "PCMA", 8000, 64000, 20, "G.711 ALaw" },
      { 18, "G729", 8000, 8000,  20, "G.729" },
    };
  
  pjsip_module PJCall::mod_siprtp_ =
    {
      NULL, NULL,			    /* prev, next.		*/
      { "mod-siprtpapp", 13 },	    /* Name.			*/
      -1,				    /* Id			*/
      PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
      NULL,			    /* load()			*/
      NULL,			    /* start()			*/
      NULL,			    /* stop()			*/
      NULL,			    /* unload()			*/
      &on_rx_request,		    /* on_rx_request()		*/
      NULL,			    /* on_rx_response()		*/
      NULL,			    /* on_tx_request.		*/
      NULL,			    /* on_tx_response()		*/
      NULL,			    /* on_tsx_state()		*/
    };
  


  PJCall::PJCall (PJSIP *sip_instance)
  {
    pj_status_t status;

    init_app ();
    /*  Init invite session module. */
    {
      pjsip_inv_callback inv_cb;
      
      /* Init the callback for INVITE session: */
      pj_bzero(&inv_cb, sizeof(inv_cb));
      inv_cb.on_state_changed = &call_on_state_changed;
      inv_cb.on_new_session = &call_on_forked;
      inv_cb.on_media_update = &call_on_media_update;
      
      /* Initialize invite session module:  */
      status = pjsip_inv_usage_init(sip_instance->sip_endpt_, &inv_cb);
      if (status != PJ_SUCCESS) 
	g_warning ("Init invite session module failed");
    }
    
    /* Register our module to receive incoming requests. */
    status = pjsip_endpt_register_module(sip_instance->sip_endpt_, &mod_siprtp_);
    if (status != PJ_SUCCESS) 
      g_warning ("Register mod_siprtp_ failed");

    // /* Init media */
    status = pjmedia_endpt_create(&sip_instance->cp_.factory, NULL, 1, &med_endpt_);
      if (status != PJ_SUCCESS) 
	g_warning ("Init media failed");

      //registering codecs
      //pjmedia_codec_g711_init(med_endpt_);
      status = PJCodec::install_codecs ();
      if (status != PJ_SUCCESS) 
       	g_warning ("Install codecs failed");
      
      /* Init calls */
      for (unsigned i=0; i<app.max_calls; ++i)
	app.call[i].index = i;
      g_print ("pj_call initialized\n");

          /* Init media transport for all calls. */
      //pj_uint16_t rtp_port = (pj_uint16_t)(app.rtp_start_port & 0xFFFE);
       for (unsigned i=0, count=0; i<app.max_calls; ++i, ++count) 
       	{
       	  unsigned j;
       	  /* Create transport for each media in the call */
       	  for (j = 0; j < PJ_ARRAY_SIZE (app.call[0].media); ++j) {
       	    /* Repeat binding media socket to next port when fails to bind
       	     * to current port number.
       	     */
       	    // int retry;
	    
       	    app.call[i].media[j].call_index = i;
       	    app.call[i].media[j].media_index = j;
	    
       	    status = -1;
	    // for (retry=0; retry<100; ++retry,rtp_port+=2)  {
	    //   struct media_stream *m = &app.call[i].media[j];
	      
	    //   status = pjmedia_transport_udp_create2(med_endpt_, 
	    //   					     "siprtp",
	    //   					     &app.local_addr,
	    //   					     rtp_port, 0, 
	    //   					     &m->transport);
	    //   if (status == PJ_SUCCESS) {
       	    //    	rtp_port += 2;
       	    //    	break;
	    //   }
	    // }
       	  }
       	}
  }
  PJCall::~PJCall ()
  {
    // unsigned i;
    // app.thread_quit = 1;
    // for (i=0; i<app.thread_count; ++i) {
    // 	if (app.sip_thread[i]) {
    // 	    pj_thread_join(app.sip_thread[i]);
    // 	    pj_thread_destroy(app.sip_thread[i]);
    // 	    app.sip_thread[i] = NULL;
    // 	}
    // }

    unsigned i;
    for (i=0; i<app.max_calls; ++i) {
      unsigned j;
      for (j=0; j<PJ_ARRAY_SIZE(app.call[0].media); ++j) {
	struct media_stream *m = &app.call[i].media[j];
	
	if (m->transport) {
	  pjmedia_transport_close(m->transport);
	  m->transport = NULL;
	}
      }
    }

    if (med_endpt_) {
      pjmedia_endpt_destroy(med_endpt_);
      med_endpt_ = NULL;
    }
    g_print ("pj_call destructed\n");
  }

  /* Callback to be called to handle incoming requests outside dialogs: */
  pj_bool_t 
  PJCall::on_rx_request (pjsip_rx_data *rdata)
  {
    g_print ("--> %s\n", __FUNCTION__);
    /* Ignore strandled ACKs (must not send respone) */
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD)
      return PJ_FALSE;
    
    /* Respond (statelessly) any non-INVITE requests with 500  */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) 
      {
      //FIXME 
      pj_str_t reason = pj_str("Unsupported Operation");
      pjsip_endpt_respond_stateless (rdata->tp_info.transport->endpt, 
				     rdata, 
       				     500, &reason,
       				     NULL, NULL);
      return PJ_TRUE;
    }
    
    /* Handle incoming INVITE */
    process_incoming_call (rdata);

    /* Done */
    return PJ_TRUE;

  }

  void
  PJCall::init_app ()
  {
    static char ip_addr[32];
    static char local_uri[64];
    
    /* Get local IP address for the default IP address */
    {
	const pj_str_t *hostname;
	pj_sockaddr_in tmp_addr;
	char *addr;

	hostname = pj_gethostname();
	pj_sockaddr_in_init(&tmp_addr, hostname, 0);
	addr = pj_inet_ntoa(tmp_addr.sin_addr);
	pj_ansi_strcpy(ip_addr, addr);
    }

    /* Init defaults */
    app.max_calls = 256;
    app.thread_count = 1;
    app.sip_port = 5072;
    app.rtp_start_port = 18000;
    app.local_addr = pj_str(ip_addr);
    app.log_level = 5;
    app.app_log_level = 3;
    app.log_filename = NULL;

    /* Default codecs: */
    app.audio_codec = audio_codecs[0];

    /* Build local URI and contact */
    pj_ansi_sprintf( local_uri, "sip:%s:%d", app.local_addr.ptr, app.sip_port);
    app.local_uri = pj_str(local_uri);
    app.local_contact = app.local_uri;
  }
  
  /* Callback to be called when invite session's state has changed: */
  void 
  PJCall::call_on_state_changed( pjsip_inv_session *inv, 
				 pjsip_event *e)
  {
    g_print ("--> %s\n", __FUNCTION__);
    struct call *call = (struct call *)inv->mod_data[mod_siprtp_.id];

    PJ_UNUSED_ARG(e);

    if (!call)
	return;

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	
	pj_time_val null_time = {0, 0};

	if (call->d_timer.id != 0) {
	  pjsip_endpt_cancel_timer(PJSIP::sip_endpt_, &call->d_timer);
	    call->d_timer.id = 0;
	}

	g_print ("Call #%d disconnected. Reason=%d (%.*s)",
		 call->index,
		 inv->cause,
		 (int)inv->cause_text.slen,
		 inv->cause_text.ptr);
    
	if (app.call_report) {
	  g_print ("Call #%d statistics:", call->index);
	  //print_call(call->index);
	}
	
	
	call->inv = NULL;
	inv->mod_data[mod_siprtp_.id] = NULL;

	// FIXME destroy_call_media(call->index);

	call->start_time = null_time;
	call->response_time = null_time;
	call->connect_time = null_time;

	++app.uac_calls;

    } else if (inv->state == PJSIP_INV_STATE_CONFIRMED) {

	pj_time_val t;

	pj_gettimeofday(&call->connect_time);
	if (call->response_time.sec == 0)
	    call->response_time = call->connect_time;

	t = call->connect_time;
	PJ_TIME_VAL_SUB(t, call->start_time);

	g_print ("Call #%d connected in %ld ms", call->index,
		  PJ_TIME_VAL_MSEC(t));

	if (app.duration != 0) {
	  call->d_timer.id = 1;
	  call->d_timer.user_data = call;
	  call->d_timer.cb = NULL;//&timer_disconnect_call;
	  
	  t.sec = app.duration;
	  t.msec = 0;
	  
	  pjsip_endpt_schedule_timer(PJSIP::sip_endpt_, &call->d_timer, &t);
	}

    } else if (	inv->state == PJSIP_INV_STATE_EARLY ||
		inv->state == PJSIP_INV_STATE_CONNECTING) {
      
      if (call->response_time.sec == 0)
	pj_gettimeofday(&call->response_time);
      
    }
    
  }
  
  /* Callback to be called when dialog has forked: */
  void 
  PJCall::call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
  {
    g_print ("--> %s\n", __FUNCTION__);
  }
  
  /* Callback to be called when SDP negotiation is done in the call: */
  void 
  PJCall::call_on_media_update( pjsip_inv_session *inv,
				pj_status_t status)
  {
    g_print ("--> %s\n", __FUNCTION__);
    struct call *call;
    //pj_pool_t *pool;
    const pjmedia_sdp_session *local_sdp, *remote_sdp;
    // struct codec *codec_desc = NULL;
    // unsigned i;

    call = (struct call *)inv->mod_data[mod_siprtp_.id];
    //pool = inv->dlg->pool;

    /* If this is a mid-call media update, then destroy existing media */
    // FIXME if (audio->thread != NULL)
    // 	destroy_call_media(call->index);


    /* Do nothing if media negotiation has failed */
    if (status != PJ_SUCCESS) {
	g_print ("SDP negotiation failed");
	return;
    }

    /* Capture stream definition from the SDP */
    pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);

    char sdpbuf1[1024], sdpbuf2[1024];
    pj_ssize_t len1, len2;
    
    //print sdp
    len1 = pjmedia_sdp_print(local_sdp, sdpbuf1, sizeof(sdpbuf1));
    if (len1 < 1) {
      g_print ("   error: printing local sdp\n");
      return;
    }
    sdpbuf1[len1] = '\0';
    len2 = pjmedia_sdp_print(remote_sdp, sdpbuf2, sizeof(sdpbuf2));
    if (len2 < 1) {
      g_print ("   error: printing sdp2");
    	return ;
    }
    sdpbuf2[len2] = '\0';
    g_print ("*NEGOCIATED* local sdp : \n%s \n\nremote sdp : \n%s \n\n ",
    	     sdpbuf1, sdpbuf2);
    
    
    
    for (uint i=0; i < call->media_count; i++)
      {
	
	struct media_stream *current_media;
	current_media = &call->media[i];
	status = stream_info_from_sdp (&current_media->si, 
				       inv->pool, 
				       med_endpt_,
				       local_sdp, 
				       remote_sdp, 
				       i);
	
	if (status != PJ_SUCCESS) {
	  g_print ("Error geting stream info from sdp");
	  return;
	}
	
	//current_media->clock_rate = current_media->si.fmt.clock_rate;
	//current_media->samples_per_frame = current_media->clock_rate * codec_desc->ptime / 1000;
	//current_media->bytes_per_frame = codec_desc->bit_rate * codec_desc->ptime / 1000 / 8;
	
	pjmedia_rtp_session_init(&current_media->out_sess, current_media->si.tx_pt, 
				 pj_rand());
	pjmedia_rtp_session_init(&current_media->in_sess, current_media->si.fmt.pt, 0);
	//  pjmedia_rtcp_init(&current_media->rtcp, "rtcp", current_media->clock_rate, 
	//  		      current_media->samples_per_frame, 0);

	current_media->shm = std::make_shared<ShmdataAnyWriter> (); 
	std::string shm_any_name ( "/tmp/switcher_sip_" + std::to_string (i));//FIXME use quiddity name
	current_media->shm->set_path (shm_any_name.c_str());
	g_message ("%s created a new shmdata any writer (%s)",  
		   shm_any_name.c_str ());

	
	/* Attach media to transport */
	status = pjmedia_transport_attach( current_media->transport, 
					   current_media, //user_data 
					   &current_media->si.rem_addr, 
					   &current_media->si.rem_rtcp, 
					   sizeof(pj_sockaddr_in),
					   &on_rx_rtp,
					   &on_rx_rtcp);
	if (status != PJ_SUCCESS) {
	  g_print ("Error on pjmedia_transport_attach()");
	  return;
	}
	
	// /* Start media thread. */
    // current_media->thread_quit_flag = 0;
	
    //FIXME time to send rtp
// #if PJ_HAS_THREADS
//     status = pj_thread_create( inv->pool, "media", &media_thread, current_media,
// 			       0, 0, &current_media->thread);
//     if (status != PJ_SUCCESS) {
// 	app_perror(THIS_FILE, "Error creating media thread", status);
// 	return;
//     }
// #endif
	
	/* Set the media as active */
	current_media->active = PJ_TRUE;
      }//end iterating media

  }


/*
 * Receive incoming call
 */
  void 
  PJCall::process_incoming_call (pjsip_rx_data *rdata)
  {
    g_print ("TODO --> %s"
	     //", name %.*s, short name %.*s, tag %.*s"
	     "\n", 
	     __FUNCTION__
     	     //, (int)rdata->msg_info.from->name.slen,
	     // rdata->msg_info.from->name.ptr,
	     // (int)rdata->msg_info.from->sname.slen,
	     // rdata->msg_info.from->sname.ptr,
	     // (int)rdata->msg_info.from->tag.slen,
	     // rdata->msg_info.from->tag.ptr
	     );
    
    unsigned i, options;
    struct call *call;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Find free call slot */
    for (i=0; i<app.max_calls; ++i) {
	if (app.call[i].inv == NULL)
	    break;
    }

    if (i == app.max_calls) {
	const pj_str_t reason = pj_str("Too many calls");
	pjsip_endpt_respond_stateless( PJSIP::sip_endpt_, rdata, 
				       500, &reason,
				       NULL, NULL);
	return;
    }

    call = &app.call[i];

     /* Parse SDP from incoming request and verify that we can handle the request. */
    pjmedia_sdp_session *offer = NULL;
    if (rdata->msg_info.msg->body) {
      pjsip_rdata_sdp_info *sdp_info;
      sdp_info = pjsip_rdata_get_sdp_info(rdata);
      offer = sdp_info->sdp;
      if (NULL == offer)      
	g_warning ("offer is null");
      status = sdp_info->sdp_err;
      if (status == PJ_SUCCESS && sdp_info->sdp == NULL)
	status = PJSIP_ERRNO_FROM_SIP_STATUS (PJSIP_SC_NOT_ACCEPTABLE);
      if (status != PJ_SUCCESS)
	g_warning ("Bad SDP in incoming INVITE");
    }

    g_print ("*** offer ****\n");
    print_sdp (offer);
    options = 0;
    status = pjsip_inv_verify_request (rdata, &options, NULL, NULL, PJSIP::sip_endpt_, &tdata);

    if (status != PJ_SUCCESS) {
	/*
	 * No we can't handle the incoming INVITE request.
	 */
	if (tdata) {
	    pjsip_response_addr res_addr;
	    
	    pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(PJSIP::sip_endpt_, &res_addr, tdata,
		NULL, NULL);
	    
	} else {
	    
	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(PJSIP::sip_endpt_, rdata, 500, NULL,
		NULL, NULL);
	}
	
	return;
    }

     
    /* Create UAS dialog */
    status = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata,
				   &app.local_contact, &dlg);
    if (status != PJ_SUCCESS) {
	const pj_str_t reason = pj_str("Unable to create dialog");
	pjsip_endpt_respond_stateless( PJSIP::sip_endpt_, rdata, 
				       500, &reason,
				       NULL, NULL);
	return;
    }

    //finding caller info
    char uristr[PJSIP_MAX_URL_SIZE];
    int len;
    len = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, 
			  rdata->msg_info.msg->line.req.uri, 
			  uristr, 
			  sizeof(uristr));
    g_print ("call req uri %.*s\n", len, uristr);
    len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, 
			  rdata->msg_info.from->uri, 
			  uristr, 
			  sizeof(uristr));
    g_print ("call from %.*s\n", len, uristr);
    len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, 
			  rdata->msg_info.to->uri, 
			  uristr, 
			  sizeof(uristr));
    g_print ("call to %.*s\n", len, uristr);
    
    //checking number of transport to create for receiving 
    //and creating transport for receiving data offered

    call->media_count = 0;
    std::vector <pjmedia_sdp_media *> media_to_receive;
    pj_uint16_t rtp_port = (pj_uint16_t)(app.rtp_start_port & 0xFFFE);//FIXME start from last attributed
    {
      unsigned int j = 0;//counting media to receive
      g_print ("media_count %d\n", offer->media_count);
      for (unsigned int media_index=0; media_index< offer->media_count; media_index++)
	{
	  //g_print ("+++++++++++++++++ attribut count %d\n", offer->media[i]->attr_count);
	  bool recv = false;
	  pjmedia_sdp_media *tmp_media = NULL;
	  for (unsigned int j=0; j<offer->media[media_index]->attr_count; j++)
	    {
	      if (0 == pj_strcmp2( &offer->media[media_index]->attr[j]->name, "sendrecv")
		  || 0 == pj_strcmp2(&offer->media[media_index]->attr[j]->name, "sendonly"))
		{
		  tmp_media = pjmedia_sdp_media_clone (dlg->pool, offer->media[media_index]);
		  tmp_media->attr[j]->name.ptr = "recvonly";
		  recv = true;
		}
	      // g_print ("name %.*s, value%.*s\n",
	      // 	       (int)offer->media[media_index]->attr[j]->name.slen,
	      // 	       offer->media[media_index]->attr[j]->name.ptr,
	      // 	       (int)offer->media[media_index]->attr[j]->value.slen,
	      // 	       offer->media[media_index]->attr[j]->value.ptr);
	    }
	  if (recv && NULL != tmp_media)
	    {
	      //adding control stream attribute
	      pjmedia_sdp_attr *attr = 
		(pjmedia_sdp_attr *)pj_pool_zalloc(dlg->pool, 
						   sizeof(pjmedia_sdp_attr));
	      std::string control_stream ("control:stream=" + std::to_string (j)); 
	      pj_strdup2(dlg->pool, &attr->name, control_stream.c_str ());
	      tmp_media->attr[tmp_media->attr_count++] = attr;

	      //saving media
	      media_to_receive.push_back (pjmedia_sdp_media_clone (dlg->pool, 
								   tmp_media));   
	      //creating transport
	      call->media_count++;
	      for (int retry=0; retry<100; ++retry,rtp_port+=2)  
		{
		  struct media_stream *m = &call->media[j];
		  m->type = std::string (offer->media[media_index]->desc.media.ptr,
					 offer->media[media_index]->desc.media.slen);
		  status = pjmedia_transport_udp_create2(med_endpt_, 
							 "siprtp",
							 &app.local_addr,
							 rtp_port, 0, 
							 &m->transport);
		  if (status == PJ_SUCCESS) {
		    rtp_port += 2;
		    break;
		  }
		}
	      j++;
	    }
	}
    }
        
    /* Create SDP */
    create_sdp (dlg->pool, call, media_to_receive, &sdp);

    g_print ("sdp created from remote\n");
    print_sdp (sdp);
    
    // pjmedia_transport_info tpinfo;
    // pjmedia_transport_info_init(&tpinfo);
    // pjmedia_transport_get_info(audio->transport, &tpinfo);
    // tpinfo.sock_info

    // pjmedia_endpt_create_sdp (med_endpt_,
    // 				 dlg->pool,
    // 				 3,
    // 				 const pjmedia_sock_info  	sock_info[],
    // 				 &sdp) 	

    /* Create UAS invite session */
    //sdp=NULL;
    status = pjsip_inv_create_uas (dlg, rdata, sdp, 0, &call->inv);
    if (status != PJ_SUCCESS) 
      {
	g_print ("********************************** error creating uas");
	pjsip_dlg_create_response (dlg, rdata, 500, NULL, &tdata);
	pjsip_dlg_send_response (dlg, pjsip_rdata_get_tsx(rdata), tdata);
	return;
      }
    else
      g_print ("********************************** success creating uas");
	  
    // const pjmedia_sdp_session *offer = NULL;
    // pjmedia_sdp_neg_get_neg_remote(call->inv->neg, &offer);

    /* Attach call data to invite session */
    call->inv->mod_data[mod_siprtp_.id] = call;

    /* Mark start of call */
    pj_gettimeofday(&call->start_time);

    /* Create 200 response .*/
    status = pjsip_inv_initial_answer (call->inv, rdata, 200, 
				       NULL, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	status = pjsip_inv_initial_answer (call->inv, rdata, 
					   PJSIP_SC_NOT_ACCEPTABLE,
					   NULL, NULL, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_inv_send_msg (call->inv, tdata); 
	else
	    pjsip_inv_terminate (call->inv, 500, PJ_FALSE);
	return;
    }

    g_print ("local sdp before sending :");
    print_sdp (sdp);

    /* Send the 200 response. */  
    status = pjsip_inv_send_msg(call->inv, tdata); 
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);

    /* Done */
  }

  /*
 * Create SDP session for a call.
 */
  pj_status_t 
  PJCall::create_sdp (pj_pool_t *pool,
		      struct call *call,
		      const std::vector<pjmedia_sdp_media *> media_to_receive,
		      pjmedia_sdp_session **p_sdp)
  {
    g_print ("%s\n", __FUNCTION__);
    pj_time_val tv;
    pjmedia_sdp_session *sdp;
    // pjmedia_sdp_media *m;
    // pjmedia_sdp_attr *attr;

    PJ_ASSERT_RETURN(pool && p_sdp, PJ_EINVAL);

    /* Create and initialize basic SDP session */
    sdp = (pjmedia_sdp_session *)pj_pool_zalloc (pool, sizeof(pjmedia_sdp_session));

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("pjsip-siprtp");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = pj_str("IN");
    sdp->origin.addr_type = pj_str("IP4");
    sdp->origin.addr = *pj_gethostname();//FIXME this should be IP address
    sdp->name = pj_str("pjsip");

    /* Since we only support one media stream at present, put the
     * SDP connection line in the session level.
     */
    sdp->conn = (pjmedia_sdp_conn *)pj_pool_zalloc (pool, sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = pj_str("IN");
    sdp->conn->addr_type = pj_str("IP4");
    sdp->conn->addr = app.local_addr;

    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;
    sdp->media_count = 0;


    for (unsigned i = 0; i < call->media_count; i++)
      {
	g_print ("make media (%s)\n",
		 call->media[i].type.c_str ());
	
	//HERE recuperer le sdp_media de l offre et y mettre le transport pour la reception
	// pjmedia_transport_info tpinfo;
	// pjmedia_transport_info_init(&tpinfo);
	// pjmedia_transport_get_info(call->media[i].transport, &tpinfo);
	// pjmedia_sdp_media *sdp_media = NULL;
	// if (0 == call->media[i].type.compare ("audio"))
	//   {
	    
	//     pj_status_t status = pjmedia_endpt_create_audio_sdp (med_endpt_,
	// 					     pool,
	// 					     &tpinfo.sock_info,
	// 					     0,
	// 					     &sdp_media);
	//      if (status != PJ_SUCCESS) 
	//        g_warning ("cannot generate sdp media line\n");
	//   }

	//getting offer media to receive
	pjmedia_sdp_media *sdp_media = media_to_receive[i];
	//getting transport info 
	pjmedia_transport_info tpinfo;
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call->media[i].transport, &tpinfo);
	sdp_media->desc.port = pj_ntohs(tpinfo.sock_info.rtp_addr_name.ipv4.sin_port);
	//put media in answer
	sdp->media[i] = sdp_media;
	sdp->media_count++;
      }
    
    // /* ----------------- Create media stream 0: */
    // /* Get transport info */
    // pjmedia_transport_info tpinfo;
    // struct media_stream *audio = &call->media[0];
    // pjmedia_transport_info_init(&tpinfo);
    // pjmedia_transport_get_info(audio->transport, &tpinfo);

    // sdp->media_count = 1;
    // m = (pjmedia_sdp_media *)pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    // sdp->media[0] = m;
    
    // /* Standard media info: */
    // m->desc.media = pj_str("audio");
    // m->desc.port = pj_ntohs(tpinfo.sock_info.rtp_addr_name.ipv4.sin_port);
    // m->desc.port_count = 1;
    // m->desc.transport = pj_str("RTP/AVP");
    
    // /* Add format and rtpmap for each codec. */
    // m->desc.fmt_count = 1;
    // m->attr_count = 0;
    
    // {
    //   pjmedia_sdp_rtpmap rtpmap;
    //   pjmedia_sdp_attr *attr;
    //   char ptstr[10];
      
    //   sprintf(ptstr, "%d", app.audio_codec.pt);
    //   pj_strdup2(pool, &m->desc.fmt[0], ptstr);
    //   rtpmap.pt = m->desc.fmt[0];
    //   rtpmap.clock_rate = app.audio_codec.clock_rate;
    //   rtpmap.enc_name = pj_str(app.audio_codec.name);
    //   rtpmap.param.slen = 0;
      
    //   pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
    //   m->attr[m->attr_count++] = attr;
    // }
    
    // /* Add sendrecv attribute. */
    // attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    // //attr->name = pj_str("sendrecv");
    // attr->name = pj_str("recvonly");
    // m->attr[m->attr_count++] = attr;
    
    // attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    // attr->name = pj_str("control:stream=0");
    // m->attr[m->attr_count++] = attr;
    
    // /* -------------- Create media stream 1: */
    // sdp->media_count++;
    //  m = (pjmedia_sdp_media *)pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    //  sdp->media[1] = m;
     
    //  /* Standard media info: */
    //  m->desc.media = pj_str("video");
    //  m->desc.port = 18002;//pj_ntohs(tpinfo.sock_info.rtp_addr_name.ipv4.sin_port);
    //  m->desc.port_count = 1;
    //  m->desc.transport = pj_str("RTP/AVP");
    
    //  /* Add format and rtpmap for each codec. */
    //  m->desc.fmt_count = 1;
    //  m->attr_count = 0;
    
    //  {
    //    pjmedia_sdp_rtpmap rtpmap;
    //    pjmedia_sdp_attr *attr;
    //    char ptstr[10];
      
    //    sprintf(ptstr, "%d", 92);
    //    pj_strdup2(pool, &m->desc.fmt[0], ptstr);
    //    rtpmap.pt = m->desc.fmt[0];
    //    rtpmap.clock_rate = 90000;
    //    rtpmap.enc_name = pj_str("theora");
    //    rtpmap.param.slen = 0;
      
    //    pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
    //    m->attr[m->attr_count++] = attr;

    //    // g_print ("+++++++++++++++++++++++++++ attr (%.*s) (%.*s)\n",
    //    // 		(int)attr->name.slen,
    //    // 		attr->name.ptr,
    //    // 		(int)attr->value.slen,
    //    // 		attr->value.ptr);
    //  }

    //   {
    //     pjmedia_sdp_attr *attr;
    // 	attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    // 	attr->name = pj_str("fmtp:92");
    // 	attr->value = pj_str ("height=576;width=706");
    // 	m->attr[m->attr_count++] = attr;
    //   }

    //  attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    //  attr->name = pj_str("control:stream=1");
    //  m->attr[m->attr_count++] = attr;
    
    //  /* Add sendrecv attribute. */
    //  attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    //  attr->name = pj_str("sendrecv");
    //  //attr->name = pj_str("recvonly");

    //  m->attr[m->attr_count++] = attr;
    //  //end media stream 1


 // #if 1
 //     /*
 //      * Add support telephony event
 //      */
 //     m->desc.fmt[m->desc.fmt_count++] = pj_str("121");
 //     /* Add rtpmap. */
 //     attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
 //     attr->name = pj_str("rtpmap");
 //     attr->value = pj_str("121 telephone-event/8000");
 //     m->attr[m->attr_count++] = attr;
 //     /* Add fmtp */
 //     attr = (pjmedia_sdp_attr *)pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
 //     attr->name = pj_str("fmtp");
 //     attr->value = pj_str("121 0-15");
 //     m->attr[m->attr_count++] = attr;
 // #endif

    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;
}

/*
 * This callback is called by media transport on receipt of RTP packet.
 */
  void 
  PJCall::on_rx_rtp(void *user_data, void *pkt, pj_ssize_t size)
  {


    struct media_stream *strm;
    pj_status_t status;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payload_len;

    strm = (struct media_stream *)user_data;
    g_print ("%s, media index %u\n", __FUNCTION__, strm->media_index);

    /* Discard packet if media is inactive */
    if (!strm->active)
      return;
    
    /* Check for errors */
    if (size < 0) {
	g_print ("RTP recv() error");
	return;
    }

    /* Decode RTP packet. */
    status = pjmedia_rtp_decode_rtp(&strm->in_sess, 
				    pkt, (int)size, 
				    &hdr, &payload, &payload_len);

    if (!strm->shm->started ())
      {
	//FIXME check for channels in fmt + all si.param + ssrc + clock-base + seqnum-base
	std::string data_type ("application/x-rtp" 
				 //+ ", media= " + current_media->type 
				 //+ ", clock-rate=" + std::to_string (current_media->si.fmt.clock_rate)
				 //+ ", payload=" + std::to_string (current_media->si.fmt.pt)
				 //+ ", encoding-name=" + std::string (current_media->si.fmt.encoding_name.ptr, current_media->si.fmt.encoding_name.len)
				 // + ", ssrc=" + 
				 // + ", clock-base="
				 // + ", seqnum-base=" + std::to_string (current_media->in_sess->seq_ctrl->base_seq)
				 );
	
	strm->shm->set_data_type (data_type);
	
	//application/x-rtp, media=(string)application, clock-rate=(int)90000, encoding-name=(string)X-GST, ssrc=(uint)4199653519, payload=(int)96, clock-base=(uint)479267716, seqnum-base=(uint)53946
	strm->shm->start ();
      }

    strm->shm->push_data (pkt, //FIXME handle that, make copy and make clock
			  (size_t) size,
			  0,
			  NULL,
			  NULL);
    

    if (status != PJ_SUCCESS) {
	g_print ("RTP decode error");
	return;
    }

    
    //PJ_LOG(4,(THIS_FILE, "Rx seq=%d", pj_ntohs(hdr->seq)));

    /* Update the RTCP session. */
    // pjmedia_rtcp_rx_rtp(&strm->rtcp, pj_ntohs(hdr->seq),
    // 			pj_ntohl(hdr->ts), payload_len);

    /* Update RTP session */
    pjmedia_rtp_session_update(&strm->in_sess, hdr, NULL);

}

/*
 * This callback is called by media transport on receipt of RTCP packet.
 */
void 
PJCall::on_rx_rtcp(void *user_data, void *pkt, pj_ssize_t size)
{
    g_print ("%s\n",__FUNCTION__);
    return;

    struct media_stream *strm;

    strm = (struct media_stream *)user_data;

    /* Discard packet if media is inactive */
    if (!strm->active)
	return;

    /* Check for errors */
    if (size < 0) {
	g_print ("Error receiving RTCP packet");
	return;
    }

    /* Update RTCP session */
    pjmedia_rtcp_rx_rtcp(&strm->rtcp, pkt, size);
}
 

  void 
  PJCall::print_sdp (const pjmedia_sdp_session *local_sdp)
  {
    char sdpbuf1[4096];
    pj_ssize_t len1;
    len1 = pjmedia_sdp_print(local_sdp, sdpbuf1, sizeof(sdpbuf1));
    if (len1 < 1) {
      g_print ("   error: printing local sdp\n");
      return;
    }
    sdpbuf1[len1] = '\0';
    g_print ("sdp : \n%s \n\n ",
	     sdpbuf1);
    
  }


/*
 * Rewrite of pjsip function in order to get more than only audio
 * (Create stream info from SDP media line.)
 */
pj_status_t 
PJCall::stream_info_from_sdp(pjmedia_stream_info *si,
			     pj_pool_t *pool,
			     pjmedia_endpt *endpt,
			     const pjmedia_sdp_session *local,
			     const pjmedia_sdp_session *remote,
			     unsigned stream_idx)
{
  //pjmedia_codec_mgr *mgr;
    const pjmedia_sdp_attr *attr;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_conn *local_conn;
    const pjmedia_sdp_conn *rem_conn;
    int rem_af, local_af;
    pj_sockaddr local_addr;
    pj_status_t status;


    /* Validate arguments: */
    PJ_ASSERT_RETURN(pool && si && local && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < local->media_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < remote->media_count, PJ_EINVAL);

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    local_conn = local_m->conn ? local_m->conn : local->conn;
    if (local_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;

    rem_conn = rem_m->conn ? rem_m->conn : remote->conn;
    if (rem_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;

    /* Media type must be audio */
    if (pj_stricmp2(&local_m->desc.media, "audio") == 0)
      si->type = PJMEDIA_TYPE_AUDIO;
    else if (pj_stricmp2(&local_m->desc.media, "video") == 0)
      si->type = PJMEDIA_TYPE_VIDEO;
    else if (pj_stricmp2(&local_m->desc.media, "application") == 0)
      si->type = PJMEDIA_TYPE_APPLICATION;
    else
      si->type = PJMEDIA_TYPE_UNKNOWN;

    /* Get codec manager. */
    //mgr = pjmedia_endpt_get_codec_mgr(endpt);

    /* Reset: */

    pj_bzero(si, sizeof(*si));

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    si->rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Transport protocol */

    /* At this point, transport type must be compatible,
     * the transport instance will do more validation later.
     */
    status = pjmedia_sdp_transport_cmp(&rem_m->desc.transport,
				       &local_m->desc.transport);
    if (status != PJ_SUCCESS)
	return PJMEDIA_SDPNEG_EINVANSTP;

    if (pj_stricmp2(&local_m->desc.transport, "RTP/AVP") == 0) {

	si->proto = PJMEDIA_TP_PROTO_RTP_AVP;

    } else if (pj_stricmp2(&local_m->desc.transport, "RTP/SAVP") == 0) {

	si->proto = PJMEDIA_TP_PROTO_RTP_SAVP;

    } else {

	si->proto = PJMEDIA_TP_PROTO_UNKNOWN;
	return PJ_SUCCESS;
    }


    /* Check address family in remote SDP */
    rem_af = pj_AF_UNSPEC();
    if (pj_stricmp2(&rem_conn->net_type, "IN")==0) {
	if (pj_stricmp2(&rem_conn->addr_type, "IP4")==0) {
	    rem_af = pj_AF_INET();
	} else if (pj_stricmp2(&rem_conn->addr_type, "IP6")==0) {
	    rem_af = pj_AF_INET6();
	}
    }

    if (rem_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_EAFNOTSUP;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(rem_af, &si->rem_addr, &rem_conn->addr,
			      rem_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Check address family of local info */
    local_af = pj_AF_UNSPEC();
    if (pj_stricmp2(&local_conn->net_type, "IN")==0) {
	if (pj_stricmp2(&local_conn->addr_type, "IP4")==0) {
	    local_af = pj_AF_INET();
	} else if (pj_stricmp2(&local_conn->addr_type, "IP6")==0) {
	    local_af = pj_AF_INET6();
	}
    }

    if (local_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_SUCCESS;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(local_af, &local_addr, &local_conn->addr,
			      local_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Local and remote address family must match */
    if (local_af != rem_af)
	return PJ_EAFNOTSUP;

    /* Media direction: */

    if (local_m->desc.port == 0 ||
	pj_sockaddr_has_addr(&local_addr)==PJ_FALSE ||
	pj_sockaddr_has_addr(&si->rem_addr)==PJ_FALSE ||
	pjmedia_sdp_media_find_attr2(local_m, "inactive", NULL)!=NULL)
    {
	/* Inactive stream. */

	si->dir = PJMEDIA_DIR_NONE;

    } else if (pjmedia_sdp_media_find_attr2(local_m, "sendonly", NULL)!=NULL) {

	/* Send only stream. */

	si->dir = PJMEDIA_DIR_ENCODING;

    } else if (pjmedia_sdp_media_find_attr2(local_m, "recvonly", NULL)!=NULL) {

	/* Recv only stream. */

	si->dir = PJMEDIA_DIR_DECODING;

    } else {

	/* Send and receive stream. */

	si->dir = PJMEDIA_DIR_ENCODING_DECODING;

    }

    /* No need to do anything else if stream is rejected */
    if (local_m->desc.port == 0) {
	return PJ_SUCCESS;
    }

    /* If "rtcp" attribute is present in the SDP, set the RTCP address
     * from that attribute. Otherwise, calculate from RTP address.
     */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
				  "rtcp", NULL);
    if (attr) {
	pjmedia_sdp_rtcp_attr rtcp;
	status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
	if (status == PJ_SUCCESS) {
	    if (rtcp.addr.slen) {
		status = pj_sockaddr_init(rem_af, &si->rem_rtcp, &rtcp.addr,
					  (pj_uint16_t)rtcp.port);
	    } else {
		pj_sockaddr_init(rem_af, &si->rem_rtcp, NULL,
				 (pj_uint16_t)rtcp.port);
		pj_memcpy(pj_sockaddr_get_addr(&si->rem_rtcp),
		          pj_sockaddr_get_addr(&si->rem_addr),
			  pj_sockaddr_get_addr_len(&si->rem_addr));
	    }
	}
    }

    if (!pj_sockaddr_has_addr(&si->rem_rtcp)) {
	int rtcp_port;

	pj_memcpy(&si->rem_rtcp, &si->rem_addr, sizeof(pj_sockaddr));
	rtcp_port = pj_sockaddr_get_port(&si->rem_addr) + 1;
	pj_sockaddr_set_port(&si->rem_rtcp, (pj_uint16_t)rtcp_port);
    }


    /* Get the payload number for receive channel. */
    /*
       Previously we used to rely on fmt[0] being the selected codec,
       but some UA sends telephone-event as fmt[0] and this would
       cause assert failure below.

       Thanks Chris Hamilton <chamilton .at. cs.dal.ca> for this patch.

    // And codec must be numeric!
    if (!pj_isdigit(*local_m->desc.fmt[0].ptr) ||
	!pj_isdigit(*rem_m->desc.fmt[0].ptr))
    {
	return PJMEDIA_EINVALIDPT;
    }

    pt = pj_strtoul(&local_m->desc.fmt[0]);
    pj_assert(PJMEDIA_RTP_PT_TELEPHONE_EVENTS==0 ||
	      pt != PJMEDIA_RTP_PT_TELEPHONE_EVENTS);
    */

    /* Get codec info and param */
    status = PJ_SUCCESS;//get_audio_codec_info_param(si, pool, mgr, local_m, rem_m);

    /* Leave SSRC to random. */
    si->ssrc = pj_rand();

    /* Set default jitter buffer parameter. */
    si->jb_init = si->jb_max = si->jb_min_pre = si->jb_max_pre = -1;

    return status;

}
  

}
