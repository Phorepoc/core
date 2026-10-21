/*
* Copyright (c) 2001-2006 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the Eclipse Public License v2.0, available at
* https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
* section 7 of the Eclipse Public License v1.0).
* This Source Code may also be made available under the following Secondary
* Licenses when the conditions for such availability set forth in the Eclipse
* Public License, v. 2.0 are satisfied: GNU General Public License, version 2
* or later.
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
* The Phorepoc Authors
*
* Description:  EAP and WLAN authentication protocols.
*
*/

/*
* %version: 22.1.3 %
*/

// This is enumeration of EAPOL source code.
#if defined(USE_EAP_MINIMUM_RELEASE_TRACES)
	#undef EAP_FILE_NUMBER_ENUM
	#define EAP_FILE_NUMBER_ENUM 147 
	#undef EAP_FILE_NUMBER_DATE 
	#define EAP_FILE_NUMBER_DATE 1127594498 
#endif //#if defined(USE_EAP_MINIMUM_RELEASE_TRACES)



#include "eap_am_tools_symbian.h"
#include "eap_am_types.h"
#include <e32math.h>
#include <utf.h>

const TUint MAX_DB_TRANSACTION_RETRY_COUNT = 10;
const u32_t EAP_TIMER_MAX_AFTER_TIME_MILLISECONDS_SYMBIAN = 2100000ul;

//--------------------------------------------------

#if defined(_WIN32) && !defined(__GNUC__)
	#pragma warning( disable : 4355 ) // 'this' : used in base member initializer list
#endif

//--------------------------------------------------

EAP_FUNC_EXPORT eap_am_tools_symbian_c::~eap_am_tools_symbian_c()
{
	// If multithreading is used the log file is not kept open all the time
	// so no need to close the handles here.
#if defined(USE_EAP_FILE_TRACE)
#if !defined (USE_MULTITHREADING)
	m_LogFile.Close();
	m_Fs.Close();
#endif //#if !defined (USE_MULTITHREADING)
#endif //#if defined(USE_EAP_FILE_TRACE)
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT eap_am_tools_symbian_c::eap_am_tools_symbian_c(eap_const_string /*pfilename*/)
	: eap_am_tools_c()
	, CTimer(CTimer::EPriorityStandard)
	, m_start_ticks(0)
	, m_directory_exists(false)
	, m_crypto(this)
	, m_timer_queue(this, EAP_TIMER_RESOLUTION)
	, m_run_thread(true)
	, m_is_valid(false)
	, m_configure_called(false)
	, iInterval(0ul)
	, iStartTime(0ul)
	, iLastTime(0ul)
	, m_logfile_open(false)	
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

#if defined(USE_EAP_HARDWARE_TRACE)
	set_trace_mask(
		eap_am_tools_c::eap_trace_mask_always
		| eap_am_tools_c::eap_trace_mask_error
		| eap_am_tools_c::eap_trace_mask_debug
		| eap_am_tools_c::eap_trace_mask_message_data);
#endif //#if defined(USE_EAP_HARDWARE_TRACE)

	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_DEFAULT,
		(EAPL("eap_am_tools_symbian_c::eap_am_tools_symbian_c()\n")));

	if (m_crypto.get_is_valid() == false)
	{
		EAP_TRACE_ERROR(
			this,
			TRACE_FLAGS_DEFAULT,
			(EAPL("ERROR: m_crypto.get_is_valid() == false\n")));
		return;
	}

	if (m_timer_queue.get_is_valid() == false)
	{
		EAP_TRACE_ERROR(
			this,
			TRACE_FLAGS_DEFAULT,
			(EAPL("ERROR: m_timer_queue.get_is_valid() == false\n")));
		return;
	}

	if (m_global_mutex.get_is_valid() == false)
	{
		EAP_TRACE_ERROR(
			this,
			TRACE_FLAGS_DEFAULT,
			(EAPL("ERROR: m_global_mutex.get_is_valid() == false\n")));
		return;
	}

	if (m_trace_mutex.get_is_valid() == false)
	{
		EAP_TRACE_ERROR(
			this,
			TRACE_FLAGS_DEFAULT,
			(EAPL("ERROR: m_trace_mutex.get_is_valid() == false\n")));
		return;
	}

	m_is_valid = true;

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::set_use_eap_milli_second_timer(
	const bool use_eap_millisecond_timer)
{
	m_timer_queue.set_use_eap_milli_second_timer(use_eap_millisecond_timer);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT bool eap_am_tools_symbian_c::get_use_eap_milli_second_timer()
{
	return m_timer_queue.get_use_eap_milli_second_timer();
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::configure()
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_DEFAULT,
		(EAPL("eap_am_tools_symbian_c::configure()\n")));

	if (m_configure_called == true)
	{
		return EAP_STATUS_RETURN(this, eap_status_ok);
	}

#if defined(USE_EAP_HARDWARE_TRACE)
	set_trace_mask(
		eap_am_tools_c::eap_trace_mask_always
		| eap_am_tools_c::eap_trace_mask_error
		| eap_am_tools_c::eap_trace_mask_debug
		| eap_am_tools_c::eap_trace_mask_message_data);
#endif //#if defined(USE_EAP_HARDWARE_TRACE)

	m_start_ticks = get_clock_ticks();
	iLastTime = m_start_ticks;

	eap_status_e status = m_crypto.configure();
	if (status != eap_status_ok)
	{
		return EAP_STATUS_RETURN(this, status);
	}

	m_crypto.add_rand_seed_hw_ticks();

	if (m_crypto.get_is_valid() == false)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_allocation_error);
	}

	if (m_timer_queue.get_is_valid() == false)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_allocation_error);
	}

	if (m_global_mutex.get_is_valid() == false)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_allocation_error);
	}

	if (m_trace_mutex.get_is_valid() == false)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_allocation_error);
	}

#if defined(USE_EAP_FILE_TRACE)
#if !defined (USE_MULTITHREADING)
	TInt result = m_Fs.Connect();	
	if (result != KErrNone)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_process_general_error);
	}
#endif //#if !defined (USE_MULTITHREADING)
#endif //#if defined(USE_EAP_FILE_TRACE)

	{
		set_use_timer_queue();
		set_use_eap_milli_second_timer(true);
		
		TRAPD(err, CTimer::ConstructL());
		if (err != KErrNone)
		{
			EAP_TRACE_ERROR(
				this,
				TRACE_FLAGS_DEFAULT,
				(EAPL("CTimer::ConstructL() failed, err %d.\n"),
				err));
			EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);	
			return EAP_STATUS_RETURN(this, eap_status_process_general_error);
		}

		CActiveScheduler::Add(this);
	}

	m_configure_called = true;

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return eap_status_ok;
}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::enter_global_mutex()
{
	EAP_ASSERT_ALWAYS_TOOLS(this, m_configure_called == true);

	m_global_mutex.mutex_enter();
}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::leave_global_mutex()
{
	m_global_mutex.mutex_leave(this);
}

//--------------------------------------------------

EAP_FUNC_EXPORT abs_eap_am_mutex_c * eap_am_tools_symbian_c::get_global_mutex()
{
	return &m_global_mutex;
}


//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::enter_trace_mutex()
{
	m_trace_mutex.mutex_enter();
}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::leave_trace_mutex()
{
	m_trace_mutex.mutex_leave(this);
}

//--------------------------------------------------

EAP_FUNC_EXPORT abs_eap_am_mutex_c * eap_am_tools_symbian_c::get_trace_mutex()
{
	return &m_trace_mutex;
}



//--------------------------------------------------

EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::get_timer_resolution_ms()
{
	enter_global_mutex();
	const u32_t res = m_timer_queue.get_timer_resolution_ms();
	leave_global_mutex();
	return res;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::set_timer_resolution_ms(const u32_t timer_resolution_ms)
{
	enter_global_mutex();
	m_timer_queue.set_timer_resolution_ms(timer_resolution_ms);
	leave_global_mutex();
}

//--------------------------------------------------

EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::pulse_timer(const u32_t elapsed_time_in_ms)
{
	return m_timer_queue.pulse_timer(elapsed_time_in_ms, true);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::sprint(TDes& /*KPrintBuf*/, eap_const_string /*format*/, ...)
{
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::snprintf(u8_t * const buffer, u32_t buffer_size, eap_format_string format, ...)
{
	if (buffer == 0)
	{
		return 0ul;
	}

	u32_t result = 0;
	m_format_buf.Copy((const TUint8 *)format);
	VA_LIST args;		// OURS: GCC refuses the brace-initialised va_list (the ARM EABI __va_list is a struct)
	VA_START(args, format);
	m_trace_buf.FormatList(m_format_buf, args);
	VA_END(args);
	result = (static_cast<u32_t>(m_trace_buf.Size()) < buffer_size) ? static_cast<u32_t>(m_trace_buf.Size()) : buffer_size;
	memmove(buffer, m_trace_buf.PtrZ(), result);

	return result;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::formatted_print(eap_format_string format, ...)
{
	EAP_UNREFERENCED_PARAMETER(format);
	
#if defined(USE_EAP_TRACE) || defined(USE_EAP_TRACE_ALWAYS)

	enter_trace_mutex();

	u64_t time_stamp = get_clock_ticks();
	TInt64 *_time_stamp = (TInt64 *)&time_stamp;		
	if (get_use_seconds_timestamp_in_traces() == true)
	{
		u64_t hw_ticks_of_second = get_clock_ticks_of_second();
		TInt64 *_hw_ticks_of_second = (TInt64 *)&hw_ticks_of_second;		
		time_stamp -= m_start_ticks;

		{
			TInt64 _seconds = (*_time_stamp) / (*_hw_ticks_of_second);
			TInt64 _remainder = (*_time_stamp) % (*_hw_ticks_of_second);
			TInt64 _multiplier((TUint)1000000ul);
			TInt64 _tmp = _multiplier * _remainder;
			TInt64 _micro_seconds = _tmp / (*_hw_ticks_of_second);
			TInt64 _div_60((TUint)60ul);

			TInt64 _minutes = _seconds / _div_60;
			_seconds = _seconds - _minutes * _div_60;
			TInt64 _hours = _minutes / _div_60;
			_minutes = _minutes - _hours* _div_60;

			_LIT8(KFormat1, "%02d:%02d:%02d.%06d:EAPOL:");

			m_trace_buf.Format(
				KFormat1,
				static_cast<TInt32>(_hours),
				static_cast<TInt32>(_minutes),
				static_cast<TInt32>(_seconds),
				static_cast<TInt32>(_micro_seconds));				
		}

	}
	else
	{
		_LIT8(KFormat2, "%08x%08x:EAPOL:");

		u32_t *time_stamp_u32_t = reinterpret_cast<u32_t *>(&time_stamp);
		m_trace_buf.Format(KFormat2, time_stamp_u32_t[1], time_stamp_u32_t[0]);		
	}

	VA_LIST args;		// OURS: GCC refuses the brace-initialised va_list (the ARM EABI __va_list is a struct)
	VA_START(args, format);
	m_format_buf.Copy((const TUint8 *)format);
	
	m_args_buf.FormatList(m_format_buf, args);
	m_trace_buf.Append(m_args_buf);			
	VA_END(args);


#if defined(USE_EAP_HARDWARE_TRACE)

	{
		#if !defined(USE_EAP_HARDWARE_TRACE_RAW_PRINT)
		{
			TInt length = m_trace_buf.Length();
			if (length >= 2ul)
			{
				--length;
				const TUint8 *last_char = m_trace_buf.Ptr() + length;

				if (last_char != 0
					&& *last_char == '\n')
				{
					// This removes the ending new line character.
					// RDebug::Print() will write new line automatically.
					m_trace_buf.SetLength(length);
				}
			}
		}
		#endif //#if !defined(USE_EAP_HARDWARE_TRACE_RAW_PRINT)
		
		m_trace_buf_16.Copy(m_trace_buf);

		#if defined(USE_EAP_HARDWARE_TRACE_RAW_PRINT)
			RDebug::RawPrint(m_trace_buf_16);
		#else
			RDebug::Print(_L("PH: %S"), &m_trace_buf_16);	// OURS: "PH:" = forwarded to PhoneLab by the kernel seam
		#endif //#if defined(USE_EAP_HARDWARE_TRACE_RAW_PRINT)
	}

#endif //#if defined(USE_EAP_HARDWARE_TRACE)


#if defined(USE_EAP_FILE_TRACE)

	#if defined (USE_MULTITHREADING)
		if (m_filename.Length() > 0ul)
		{
			RFs session;
			
			TInt result = session.Connect();	
			if (result != KErrNone)
			{
				leave_trace_mutex();
				EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
				return;
			}	
			
			TInt err = m_LogFile.Open(session, m_filename, EFileWrite | EFileShareAny);
			if (err != KErrNone)
			{			
				session.Close();
				leave_trace_mutex();
				EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
				return;
			}
			m_logfile_open = true;
		}
	#endif //#if defined (USE_MULTITHREADING)


	if (m_logfile_open == true)
	{
		//  possible errors in the write because we can't do anything if
		// for example disk is full.
		TInt pos = 0;
		
		// Seek to the end of file for appending
		m_LogFile.Seek(ESeekEnd, pos);
		m_LogFile.Write(m_trace_buf);
		// This flush crashed sometimes for an unknown reason...
		m_LogFile.Flush();
	}
		
	#if defined (USE_MULTITHREADING)
		if (m_logfile_open == true)
		{
			m_logfile_open = false;
			m_LogFile.Close();
		}
		session.Close();
	#endif //#if defined (USE_MULTITHREADING)

#endif //#if defined(USE_EAP_FILE_TRACE)


	leave_trace_mutex();

#endif //#if defined(USE_EAP_TRACE) || defined(USE_EAP_TRACE_ALWAYS)

}


//--------------------------------------------------

//
EAP_FUNC_EXPORT bool eap_am_tools_symbian_c::get_timer_queue_is_empty()
{
	return m_timer_queue.get_timer_queue_is_empty();
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT bool eap_am_tools_symbian_c::get_is_timer_thread_active()
{
	return m_run_thread;
}


//--------------------------------------------------

//
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::set_trace_file_name(
	const eap_variable_data_c * const trace_output_file)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	EAP_UNREFERENCED_PARAMETER(trace_output_file);
	
#if defined(USE_EAP_FILE_TRACE)
#if defined(USE_EAP_TRACE) || defined(USE_EAP_TRACE_ALWAYS)
	TBuf8<64> tmpFilename((TUint8 *)trace_output_file->get_data(trace_output_file->get_data_length()));
	tmpFilename.SetLength(trace_output_file->get_data_length());
	enter_trace_mutex();

	m_filename.Copy(tmpFilename);

#if defined (USE_MULTITHREADING)
	TInt result = m_Fs.Connect();	
	if (result != KErrNone)
	{
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return convert_am_error_to_eapol_error(result);
	}
#endif //#if defined (USE_MULTITHREADING)

	TInt err(KErrPathNotFound);

	if (m_filename.Length() > 0ul)
	{
		err = m_LogFile.Create(m_Fs, m_filename, EFileWrite | EFileShareAny);
		if (err == KErrAlreadyExists)
		{
			err = m_LogFile.Open(m_Fs, m_filename, EFileWrite | EFileShareAny);
		}
		else if (err == KErrPathNotFound)
		{
#if !defined(USE_EAP_HARDWARE_TRACE)
			// No directory neither RDEBUG -> Disable traces
			set_trace_mask(eap_am_tools_c::eap_trace_mask_none);
#endif //#if !defined(USE_EAP_HARDWARE_TRACE)
		}
	}
	
	if (err != KErrNone)
	{
		m_logfile_open = false;
#if defined (USE_MULTITHREADING)
		m_Fs.Close();
#endif //#if defined (USE_MULTITHREADING)
		leave_trace_mutex();
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return convert_am_error_to_eapol_error(err);
	}
	else
	{
		// Trace file opened OK.
		m_logfile_open = true;
	}

#if defined (USE_MULTITHREADING)
	m_LogFile.Close();	
	m_logfile_open = false;
	m_Fs.Close();
#endif //#if defined (USE_MULTITHREADING)

	leave_trace_mutex();
	
#endif //#if defined(USE_EAP_TRACE) || defined(USE_EAP_TRACE_ALWAYS)
#endif //#if defined(USE_EAP_FILE_TRACE)


	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return eap_status_ok;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::set_max_trace_file_size(const u32_t max_trace_file_size)
{
#if defined(USE_EAP_TRACE_FILE_MAX_SIZE)
	m_max_trace_file_size = max_trace_file_size;
#else
	EAP_UNREFERENCED_PARAMETER(max_trace_file_size);
#endif //#if defined(USE_EAP_TRACE_FILE_MAX_SIZE)
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::memmove(void *dest, const void *src, const u32_t count)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	Mem::Copy(dest, src, count);
	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT i32_t eap_am_tools_symbian_c::memcmp(const void * const dest, const void * const src, const u32_t count)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return Mem::Compare((const TUint8*)dest, count, (const TUint8*)src, count);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void eap_am_tools_symbian_c::memset(void * const src, const i32_t fill_byte, const u32_t count)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	Mem::Fill(src, count, fill_byte);
	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void *eap_am_tools_symbian_c::memchr(
		const void *buf,
		u8_t character,
		u32_t count)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	const u8_t * const tmp = reinterpret_cast<const u8_t *>(buf);
	
	for (u32_t ind = 0u; ind < count; ind++)
	{
		if (tmp[ind] == character)
		{
			return reinterpret_cast<void *>(const_cast<u8_t *>(&(tmp[ind])));
		}
	}

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);

	return 0;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT void *eap_am_tools_symbian_c::memrchr(
		const void *buf,
		u8_t character,
		u32_t count)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	const u8_t * const tmp = reinterpret_cast<const u8_t *>(buf);
	
	for (i32_t ind = count-1ul; ind >= 0; ind--)
	{
		if (tmp[ind] == character)
		{
			return reinterpret_cast<void *>(const_cast<u8_t *>(&(tmp[ind])));
		}
	}

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);

	return 0;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::strlen(
		eap_const_string string)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	u32_t count = config_strlen(string);

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return count;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::config_strlen(
		eap_config_string string)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	u32_t count = 0;
	const u8_t *tmp = reinterpret_cast<const u8_t *>(string);
	while((*tmp) != 0)
	{
		tmp++;
		count++;
	}
	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return count;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT abs_eap_am_crypto_c * eap_am_tools_symbian_c::get_crypto()
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return &m_crypto;
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u64_t eap_am_tools_symbian_c::get_hardware_ticks()
{
	return get_clock_ticks();
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u64_t eap_am_tools_symbian_c::get_hardware_ticks_of_second()
{
	return get_clock_ticks_of_second();
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u64_t eap_am_tools_symbian_c::get_clock_ticks()
{
	#if defined(__SYMBIAN32__)
		TTime time;
		time.UniversalTime();
		return *(reinterpret_cast<u64_t *>(&time));
	#else
		#error get_clock_ticks() not supported, please do something
	#endif
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT u64_t eap_am_tools_symbian_c::get_clock_ticks_of_second()
{
	return 1000000u;
}

//--------------------------------------------------

/**
 * This function adds EAP timer to queue and sets the platform specific timer for first timer in the queue.
 */
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::am_set_timer(
	abs_eap_base_timer_c * const initializer, 
	const u32_t id, 
	void * const data,
	const u32_t p_time_ms)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	if (get_use_timer_queue() == false)
	{
		// We do not need timers.
		initializer->timer_delete_data(id, data);

		EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("end am_set_timer().\n")));
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_not_supported);
	}
	else
	{
		const eap_status_e status = m_timer_queue.set_timer(
			initializer,
			id,
			data,
			p_time_ms);
			
		if (IsActive())
		{
			// This call cancels the current symbian timer.
			// We will start new symbian timer with new
			// timeout read from timer queue.
			Cancel();
		}
		
		u64_t currentTime = get_clock_ticks();
		u32_t tics_of_millisecond = static_cast<u32_t>(get_hardware_ticks_of_second())/1000ul;
		u32_t spend_time = static_cast<u32_t>(currentTime - iLastTime)/tics_of_millisecond;

		iLastTime = currentTime;
		
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_status_e eap_am_tools_symbian_c::am_set_timer(): pulse_timer(%d ms).\n"),
			spend_time));

		u32_t next_timeout_milliseconds = m_timer_queue.pulse_timer(spend_time, false);

		After(limit_microsecond_timeout(next_timeout_milliseconds));
		
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("end eap_am_tools_symbian_c::am_set_timer(): After(%d millisecond).\n"),
			next_timeout_milliseconds));

		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, status);
	}
}

//--------------------------------------------------

//
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::am_cancel_timer(
	abs_eap_base_timer_c * const initializer, 
	const u32_t id)
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	if (get_use_timer_queue() == false)
	{
		// We do not need timers.
		EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("end am_cancel_timer().\n")));
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_not_supported);
	}
	else
	{
		const eap_status_e status = m_timer_queue.cancel_timer(initializer, id);

		EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("end am_cancel_timer().\n")));
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return status;
	}
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::am_cancel_all_timers()
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);

	EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("begin am_cancel_all_timers().\n")));

	if (get_use_timer_queue() == false)
	{
		// We do not need timers.
		EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("end am_cancel_all_timers().\n")));
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return EAP_STATUS_RETURN(this, eap_status_not_supported);
	}
	else
	{
		enter_global_mutex();
		const eap_status_e status = m_timer_queue.cancel_all_timers();
		leave_global_mutex();

		EAP_TRACE_DEBUG(this, TRACE_FLAGS_TIMER, (EAPL("end am_cancel_all_timers().\n")));
		EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
		return status;
	}
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::re_activate_timer_queue()
{
	EAP_TRACE_BEGIN(this, TRACE_FLAGS_DEFAULT);
	EAP_TRACE_DEBUG(this, TRACE_FLAGS_DEFAULT, (EAPL("re_activate_timer_queue().\n")));

	const eap_status_e status =  m_timer_queue.re_activate_timer_queue();

	EAP_TRACE_END(this, TRACE_FLAGS_DEFAULT);
	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::enter_crypto_cs()
{

}

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::leave_crypto_cs()
{

}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::timer_sleep(u32_t milli_seconds)
{
	sleep(milli_seconds);
}

//--------------------------------------------------

EAP_FUNC_EXPORT void eap_am_tools_symbian_c::sleep(u32_t milli_seconds)
{
	User::After(limit_microsecond_timeout(milli_seconds));
}

//--------------------------------------------------
EAP_FUNC_EXPORT u32_t eap_am_tools_symbian_c::get_gmt_unix_time()
{
	_LIT(KStart, "19700000:000000.000000");
	TTime start(KStart);
	TTime now;
	now.UniversalTime();
	TTimeIntervalSeconds interval;
	now.SecondsFrom(start, interval);
	return interval.Int();
}
//--------------------------------------------------

EAP_FUNC_EXPORT bool eap_am_tools_symbian_c::get_is_valid() const
{
	return m_is_valid;
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::convert_am_error_to_eapol_error(const i32_t aErr)
{
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_DEFAULT,
		(EAPL("eap_am_tools_symbian_c::convert_am_error_to_eapol_error: error=%d\n"),
		aErr));

	eap_status_e status;
	switch (aErr)
	{
	case KErrNone:
		status = eap_status_ok;
		break;

	case KErrNoMemory:
		status = eap_status_allocation_error;
		break;

	case KErrNotSupported:
		status = eap_status_not_supported;
		break;
	
	case KErrBadHandle:
		status = eap_status_illegal_handle;
		break;
		
	case KErrCompletion:
		status = eap_status_pending_request;
		break;

	case KErrNotFound:
		status = eap_status_illegal_configure_field;
		break;

	case KErrGeneral:
	case KErrUnknown:
	case KErrCancel:
	case KErrArgument:
	case KErrTotalLossOfPrecision:
	case KErrOverflow:
	case KErrUnderflow:
	case KErrAlreadyExists:
	case KErrPathNotFound:
	case KErrDied:
	case KErrInUse:
	case KErrServerTerminated:
	case KErrServerBusy:
	case KErrNotReady:	
	case KErrCorrupt:
	case KErrAccessDenied:
	case KErrLocked:
	case KErrWrite:
	case KErrDisMounted:
	case KErrEof:
	case KErrDiskFull:
	case KErrBadDriver:
	case KErrBadName:
	case KErrCommsLineFail:
	case KErrCommsFrame:
	case KErrCommsOverrun:
	case KErrCommsParity:
	case KErrTimedOut:
	case KErrCouldNotConnect:
	case KErrCouldNotDisconnect:
	case KErrDisconnected:
	case KErrBadLibraryEntryPoint:
	case KErrBadDescriptor:
	case KErrAbort:
	case KErrTooBig:
	case KErrDivideByZero:
	case KErrBadPower:
	case KErrDirFull:
	case KErrHardwareNotAvailable:
	default:
		status = eap_status_process_general_error;
		break;		
	}
	return status;
}

EAP_FUNC_EXPORT i32_t eap_am_tools_symbian_c::convert_eapol_error_to_am_error(eap_status_e aErr)
{
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_DEFAULT,
		(EAPL("eap_am_tools_symbian_c::convert_am_error_to_eapol_error: error=%d\n"),
		aErr));

	TInt status;
	switch (aErr)
	{
	case eap_status_ok: 
	case eap_status_success:
		status = KErrNone;
		break;
	
	case eap_status_allocation_error: 
		status = KErrNoMemory;
		break;

	case eap_status_not_supported: 
		status = KErrNotSupported;
		break;


	case eap_status_illegal_handle:
		status = KErrBadHandle;
		break;

	case eap_status_pending_request:
		status = KErrCompletion;
		break;

	case eap_status_illegal_configure_field:
		status = KErrNotFound;
		break;

	case eap_status_completed_request:
	case eap_status_drop_packet_quietly:	
	case eap_status_process_general_error: 
	case eap_status_type_does_not_exists_error: 
	case eap_status_process_illegal_packet_error:
	case eap_status_ethernet_type_not_supported:
	case eap_status_illegal_eap_code:
	case eap_status_illegal_eap_type:
	case eap_status_illegal_eap_identity:
	case eap_status_authentication_failure:
	case eap_status_encryption_failure:
	case eap_status_illegal_padding:
	case eap_status_randomize_failure:
	case eap_status_handler_exists_error:
	case eap_status_handler_does_not_exists_error:
	case eap_status_wrong_sae_state:
	case eap_status_wrong_eapol_version:
	case eap_status_wrong_eapol_type:
	case eap_status_header_corrupted:
	case eap_status_wrong_sae_sequence_number:
	case eap_status_wrong_isakmp_header_version:
	case eap_status_wrong_isakmp_exchange_type:
	case eap_status_wrong_isakmp_flags:
	case eap_status_wrong_isakmp_message_id:
	case eap_status_wrong_isakmp_cookie:
	case eap_status_unsupported_isakmp_payload:
	case eap_status_key_error:
	case eap_status_too_many_offers:
	case eap_status_send_failed:
	case eap_status_data_length_not_aligned_to_block_size:
	case eap_status_wrong_network_id:	
	case eap_status_wrong_sae_header_version:
	case eap_status_wrong_sae_exchange_type:
	case eap_status_wrong_sae_flags:
	case eap_status_unsupported_sae_payload:
	case eap_status_wrong_sae_cookie:
	case eap_status_illegal_encryption_parameter_size:
	case eap_status_state_reference_count_not_zero:
	case eap_status_illegal_nai:
	case eap_status_illegal_nai_payload:
	case eap_status_illegal_data_payload:
	case eap_status_illegal_payload:
	case eap_status_illegal_hashed_index:
	case eap_status_wrong_gsmsim_state:
	case eap_status_unsupported_gsmsim_payload:
	case eap_status_gsmsim_triplet_query_failed:
	case eap_status_illegal_index:
	case eap_status_timed_out:
	case eap_status_wrong_eap_subtype:
	case eap_status_exit_test:
	case eap_status_no_matching_protocol_version:
	default:
		status = KErrGeneral;
		break;	
	}
	
	return status;
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::begin_db_transaction(RDbNamedDatabase& aDatabase)
{
	TUint retries(0);
	TInt result;
	eap_status_e status(eap_status_access_denied);

	if (aDatabase.InTransaction())
	{
		// Previous database operation failed
		aDatabase.Rollback();
	}
	
	// Seed the PRNG with time
	TTime now;
	now.HomeTime();
	TInt64 seed = now.Int64();
	TInt randomWait; 
	
	// Try to begin transaction MAX_DB_TRANSACTION_RETRY_COUNT times
	while (retries < MAX_DB_TRANSACTION_RETRY_COUNT)
	{
		result = aDatabase.Begin();
		if (result == KErrNone)
		{
			// We got read lock to database!
			status = eap_status_ok;
			break;
		}
		EAP_TRACE_DEBUG(this, TRACE_FLAGS_DEFAULT, (EAPL("begin_db_transaction(): database locked.\n")));
		retries++;
		randomWait = Math::Rand(seed);
		
		// Wait 0 - 524287 microseconds
		randomWait = randomWait & 0x7ffff;
		User::After(randomWait);
	}
	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::begin_db_update(RDbView& aView)
{
	TUint retries(0);
	eap_status_e status(eap_status_access_denied);

	// Seed the PRNG with time
	TTime now;
	now.HomeTime();
	TInt64 seed = now.Int64();
	
	TInt randomWait; 
	TInt err;	
	// Try to begin transaction MAX_DB_TRANSACTION_RETRY_COUNT times
	while (retries < MAX_DB_TRANSACTION_RETRY_COUNT)
	{
		TRAP(err, aView.UpdateL());
		if (err == KErrNone)
		{
			// We got write lock to database!
			status = eap_status_ok;
			break;
		}
		EAP_TRACE_DEBUG(this, TRACE_FLAGS_DEFAULT, (EAPL("begin_db_update(): database locked.\n")));
		retries++;
		randomWait = Math::Rand(seed);
		
		// Wait 0 - 524287 microseconds
		randomWait = randomWait & 0x7ffff;
		User::After(randomWait);
	}
	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::begin_db_delete(RDbView& aView)
{
	TUint retries(0);
	eap_status_e status(eap_status_access_denied);
	
	// Seed the PRNG with time
	TTime now;
	now.HomeTime();
	TInt64 seed = now.Int64();
	
	TInt randomWait; 
	TInt err;	
	// Try to begin transaction MAX_DB_TRANSACTION_RETRY_COUNT times
	while (retries < MAX_DB_TRANSACTION_RETRY_COUNT)
	{
		TRAP(err, aView.DeleteL());
		if (err == KErrNone)
		{
			// We got write lock to database!
			status = eap_status_ok;
			break;
		}
		EAP_TRACE_DEBUG(this, TRACE_FLAGS_DEFAULT, (EAPL("begin_db_delete(): database locked.\n")));
		retries++;
		randomWait = Math::Rand(seed);
		
		// Wait 0 - 524287 microseconds
		randomWait = randomWait & 0x7ffff;
		User::After(randomWait);
	}
	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

/// This function must call eap_am_tools_c::shutdown_am_tools().
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::shutdown()
{
	EAP_TRACE_ALWAYS(
		this, 
		TRACE_FLAGS_DEFAULT, 
		(EAPL("eap_am_tools_symbian_c::shutdown(): this = 0x%08x => 0x%08x, "),
		 this,
		 dynamic_cast<abs_eap_base_timer_c *>(this)));

	StopTimer();
	
	return eap_am_tools_c::shutdown_am_tools();
}

//--------------------------------------------------

/// This function converts unicode string to UTF8 string.
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::convert_unicode_to_utf8(
	eap_variable_data_c & dest,
	const eap_variable_data_c & src)
{
	eap_status_e status = eap_status_allocation_error;
	
	HBufC8* outputBuffer = NULL;
	// src is unicode string, length = size / 2, one utf8 char takes 1...6 bytes
	TRAPD(err, outputBuffer = HBufC8::NewL((src.get_data_length() / 2) * 6))
	if (err != KErrNone)
	{
		status = convert_am_error_to_eapol_error(err);
		return EAP_STATUS_RETURN(this, status);
	} 

	const TPtrC16 unicode(
		reinterpret_cast<unsigned short *>(src.get_data(src.get_data_length())),
		src.get_data_length() / 2); // Length in unicode characters

	TPtr8 utf8 = outputBuffer->Des();

	CnvUtfConverter::ConvertFromUnicodeToUtf8(utf8, unicode);

	status = dest.set_copy_of_buffer(outputBuffer->Ptr(), outputBuffer->Length());

	delete outputBuffer;

	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

/// This function converts UTF8 string to unicode string.
EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::convert_utf8_to_unicode(
	eap_variable_data_c & dest,
	const eap_variable_data_c & src)
{
	eap_status_e status = eap_status_allocation_error;
	
	HBufC16* outputBuffer = NULL;
	// src is UTF8 string, unicode max length is then the length of UTF8 string.
	// NOTE, HBufC16 length means count of 16-bit objects.
	TRAPD(err, outputBuffer = HBufC16::NewL(src.get_data_length()))
	if (err != KErrNone)
	{
		status = convert_am_error_to_eapol_error(err);
		return EAP_STATUS_RETURN(this, status);
	} 

	const TPtrC8 utf8(
		(src.get_data(src.get_data_length())),
		src.get_data_length()); // Length in bytes

	TPtr16 unicode = outputBuffer->Des();

	CnvUtfConverter::ConvertToUnicodeFromUtf8(unicode, utf8);

	status = dest.set_copy_of_buffer(outputBuffer->Ptr(), outputBuffer->Size());

	delete outputBuffer;

	return EAP_STATUS_RETURN(this, status);
}

//--------------------------------------------------

EAP_FUNC_EXPORT eap_status_e eap_am_tools_symbian_c::getenv(
	const eap_variable_data_c * const /* environment_variable_name */,
	eap_variable_data_c * const /* environment_variable_value */)
{
	return eap_status_not_supported;
}

//--------------------------------------------------

EAP_FUNC_EXPORT bool eap_am_tools_symbian_c::isspace(const u8_t character)
{
	if (character >= 0x09 && character <= 0x0D
		|| character == 0x20)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//--------------------------------------------------

EAP_FUNC_EXPORT u64_struct eap_am_tools_symbian_c::u64_t_to_u64_struct(const u64_t value)
{
	return *reinterpret_cast<const u64_struct *>(&value);
}

//--------------------------------------------------

EAP_FUNC_EXPORT u64_t eap_am_tools_symbian_c::u64_struct_to_u64_t(const u64_struct value)
{
	return *reinterpret_cast<const u64_t *>(&value);
}

//--------------------------------------------------
//--------------------------------------------------
//--------------------------------------------------

void eap_am_tools_symbian_c::StartTimer(const TUint aInterval)
{	
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_TIMER,
		(EAPL("eap_am_tools_symbian_c::StartTimer().\n")));

	iInterval = aInterval;
	iStartTime = get_clock_ticks();
	
	iLastTime = iStartTime;
	
	// Start the timer
	After(limit_microsecond_timeout(iInterval));
}

//--------------------------------------------------

void eap_am_tools_symbian_c::StopTimer()
{
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_TIMER,
		(EAPL("eap_am_tools_symbian_c::StopTimer().\n")));

	Cancel();
}

//--------------------------------------------------

TBool eap_am_tools_symbian_c::TimerRunning()
{
	if (!IsActive())
	{
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_am_tools_symbian_c::TimerRunning(): EFalse.\n")));

		return EFalse;
	}
	else
	{
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_am_tools_symbian_c::TimerRunning(): ETrue.\n")));

		return ETrue;
	}
}

//--------------------------------------------------

u32_t eap_am_tools_symbian_c::limit_microsecond_timeout(u32_t next_timeout_millisecond)
{
	if (next_timeout_millisecond > EAP_TIMER_MAX_AFTER_TIME_MILLISECONDS_SYMBIAN)
	{
		next_timeout_millisecond = EAP_TIMER_MAX_AFTER_TIME_MILLISECONDS_SYMBIAN;
	}

	u32_t next_timeout_microsecond = next_timeout_millisecond * 1000ul;

	return (next_timeout_microsecond);
}

//--------------------------------------------------

void eap_am_tools_symbian_c::RunL()
{
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_TIMER,
		(EAPL("eap_am_tools_symbian_c::RunL().\n")));

	u32_t next_timeout_milliseconds = iInterval;

	enter_global_mutex();
	if (get_is_timer_thread_active())
	{
		u64_t currentTime = get_clock_ticks();
		u32_t tics_of_millisecond = static_cast<u32_t>(get_hardware_ticks_of_second())/1000ul;
		u32_t spend_time = static_cast<u32_t>(currentTime - iLastTime)/tics_of_millisecond;

		iLastTime = currentTime;
		
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_am_tools_symbian_c::RunL(): pulse_timer(%d ms).\n"),
			spend_time));

		next_timeout_milliseconds = m_timer_queue.pulse_timer(spend_time, true);
	}
	leave_global_mutex();

	// Setup timer again (if somebody inside pulse_timer has not already done it...)
	if (!IsActive())
	{
		After(limit_microsecond_timeout(next_timeout_milliseconds));
		
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_am_tools_symbian_c::RunL(): After(%d millisecond).\n"),
			next_timeout_milliseconds));
	}
	else
	{
		EAP_TRACE_DEBUG(
			this,
			TRACE_FLAGS_TIMER,
			(EAPL("eap_am_tools_symbian_c::RunL(): is active.\n")));
	}
}

//--------------------------------------------------

void eap_am_tools_symbian_c::DoCancel()
{
	EAP_TRACE_DEBUG(
		this,
		TRACE_FLAGS_TIMER,
		(EAPL("eap_am_tools_symbian_c::DoCancel().\n")));

	  // Base class
	CTimer::DoCancel();
}

//--------------------------------------------------
//--------------------------------------------------
//--------------------------------------------------

EAP_FUNC_EXPORT_INTERFACE abs_eap_am_tools_c * abs_eap_am_tools_c::new_abs_eap_am_tools_c()
{
	abs_eap_am_tools_c *am_tools = new eap_am_tools_symbian_c(EAP_DEFAULT_TRACE_FILE);

	if (am_tools != 0)
	{
		eap_status_e status = am_tools->configure();

		if (status != eap_status_ok)
		{
			abs_eap_am_tools_c::delete_abs_eap_am_tools_c(am_tools);
			am_tools = 0;
		}
		else
		{
			EAP_TRACE_DEBUG(
				am_tools,
				TRACE_FLAGS_TIMER,
				(EAPL("abs_eap_am_tools_c::new_abs_eap_am_tools_c() => 0x%08x success\n"),
				am_tools));
		}
	}

	return am_tools;
}

//--------------------------------------------------

EAP_FUNC_EXPORT_INTERFACE void abs_eap_am_tools_c::delete_abs_eap_am_tools_c(abs_eap_am_tools_c * const am_tools)
{
	EAP_TRACE_DEBUG(
		am_tools,
		TRACE_FLAGS_TIMER,
		(EAPL("abs_eap_am_tools_c::delete_abs_eap_am_tools_c(0x%08x)\n"),
		am_tools));

	(void)am_tools->shutdown();

	delete am_tools;
}

//--------------------------------------------------
//--------------------------------------------------
//--------------------------------------------------




// End of file
