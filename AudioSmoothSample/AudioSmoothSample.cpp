// AudioSmoothSample.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <Windows.h>

class _CiAutoLock
{
public:
	_CiAutoLock(CRITICAL_SECTION *pCS)
	{
		m_pCS = pCS;
		if (NULL != m_pCS)
		{
			EnterCriticalSection(m_pCS);
		}
	}

	virtual ~_CiAutoLock(void)
	{
		Unlock();
	}

	void Unlock()
	{
		if (NULL != m_pCS)
		{
			LeaveCriticalSection(m_pCS);    // only release, but not delete it
			m_pCS = NULL;
		}
	}

protected:
	CRITICAL_SECTION * m_pCS;
};

class AudioSmooth
{
public:
	AudioSmooth(unsigned int uSampleBits, unsigned int uMaxSize)
	{
		m_uSampleBytes = uSampleBits / 8;
		m_uMaxSize = uMaxSize;
		m_bInFade = false;
		m_nDelta = 0;
		m_uAmpPos = 0;
		m_uFilledInd = 0;
		memset(m_pBufferArr, 0, sizeof(m_pBufferArr));
		InitializeCriticalSection(&m_csProct);
		PrepareBuf(m_uMaxSize);
	}
	~AudioSmooth()
	{
		for (size_t i = 0; i < 3; i++)
		{
			if (m_pBufferArr[i])
			{
				delete[]m_pBufferArr[i];
				m_pBufferArr[i] = NULL;
			}
		}
		DeleteCriticalSection(&m_csProct);
	}
public:
	int SmoothInput(unsigned char *pSrc, unsigned int uSize)
	{
		if (uSize > m_uMaxSize)
			return -1;

		_CiAutoLock lock(&m_csProct);

		if (m_bInFade)
		{
			long lAmpEnd = ((long)m_uAmpPos) + m_nDelta;
			if (lAmpEnd < 0)
			{
				lAmpEnd = 0;
				m_bInFade = false;
			}
			if (lAmpEnd > 100)
			{
				lAmpEnd = 100;
				m_bInFade = false;
			}
			doFade(pSrc, uSize, m_uAmpPos, (unsigned int)lAmpEnd);
			m_uAmpPos = (unsigned int)lAmpEnd;
		}
		else
			printf("");

		if (m_uFilledInd == 0 || m_uFilledInd == 2)
		{
			memcpy(m_pBufferArr[1], pSrc, uSize);
			m_uFilledInd = 1;
		}
		else
		{
			memcpy(m_pBufferArr[2], pSrc, uSize);
			m_uFilledInd = 2;
		}
		return (int)uSize;
	}
	int SmoothOutput(unsigned char *pDst, unsigned int uSize, unsigned int uStartAmp, unsigned int uEndAmp)
	{
		if (uSize > m_uMaxSize)
			return -1;

		_CiAutoLock lock(&m_csProct);

		if (m_uFilledInd)
			doFade(m_pBufferArr[m_uFilledInd], uSize, 100, 0);
		memcpy(pDst, m_pBufferArr[m_uFilledInd], uSize);
		m_uFilledInd = 0;
		return (int)uSize;
	}
	bool SetFade(int nDelta)
	{
		_CiAutoLock lock(&m_csProct);

		m_nDelta = nDelta;
		if (m_nDelta != 0)
			m_bInFade = true;
		else
			m_bInFade = false;
		if (m_nDelta > 0)
			m_uAmpPos = 0;
		else
			m_uAmpPos = 100;
		return m_bInFade;
	}
	bool PrepareBuf(unsigned int uSize)
	{
		_CiAutoLock lock(&m_csProct);

		if (uSize > m_uMaxSize)
		{
			for (size_t i = 0; i < 3; i++)
			{
				if (m_pBufferArr[i])
				{
					delete[]m_pBufferArr[i];
					m_pBufferArr[i] = NULL;
				}
			}
			m_uMaxSize = uSize;
		}
		for (size_t i = 0; i < 3; i++)
		{
			if (m_pBufferArr[i] == NULL)
			{
				m_pBufferArr[i] = new unsigned char[m_uMaxSize];
				if (m_pBufferArr[i] == NULL)
				{
					//EPrint("AudioSmooth buffer created failed!");
					return false;
				}
				memset(m_pBufferArr[i], 0, m_uMaxSize);
			}
		}
		return true;
	}
private:
	int doFade(unsigned char *pBuf, unsigned int uSize, unsigned int uStartAmp, unsigned int uEndAmp)
	{
		if (m_uSampleBytes == 1)
		{
			return doFade8(pBuf, uSize, uStartAmp, uEndAmp);
		}
		else if (m_uSampleBytes == 2)
		{
			return doFade16(pBuf, uSize, uStartAmp, uEndAmp);
		}
		return 0;
	}

	inline int doFade8(unsigned char *pBuf, unsigned int uSize, unsigned int uStartAmp, unsigned int uEndAmp)
	{
		if (uSize%m_uSampleBytes)
		{
			uSize = uSize / m_uSampleBytes * m_uSampleBytes;
		}

		bool bPositive = (uEndAmp >= uStartAmp);
		unsigned int uSteps = 0;
		if (uStartAmp != uEndAmp)
			uSteps = bPositive ? uSize / (uEndAmp - uStartAmp) : uSize / (uStartAmp - uEndAmp);

		unsigned int uCount = 0;
		long lAmp = uStartAmp;
		size_t i = 0;
		for (; i < uSize; i++)
		{
			uCount++;
			if ((unsigned int)lAmp != uEndAmp && uCount > uSteps)
			{
				bPositive ? lAmp++ : lAmp--;
				uCount = 0;
			}
			char cPCM = ((char *)pBuf)[i];
			cPCM = (char)(lAmp*((long)cPCM) / 100);
			*(char *)(&pBuf[i]) = cPCM;
		}
		return (int)i;
	}

	inline int doFade16(unsigned char *pBuf, unsigned int uSize, unsigned int uStartAmp, unsigned int uEndAmp)
	{
		if (uSize%m_uSampleBytes)
		{
			uSize = uSize / m_uSampleBytes * m_uSampleBytes;
		}

		bool bPositive = (uEndAmp >= uStartAmp);
		unsigned int uSteps = 0;
		if (uStartAmp != uEndAmp)
			uSteps = bPositive ? uSize / m_uSampleBytes / (uEndAmp - uStartAmp) : uSize / m_uSampleBytes / (uStartAmp - uEndAmp);

		unsigned int uCount = 0;
		long lAmp = uStartAmp;
		size_t i = 0;
		for (; i < uSize; i += m_uSampleBytes)
		{
			uCount++;
			if ((unsigned int)lAmp != uEndAmp && uCount > uSteps)
			{
				bPositive ? lAmp++ : lAmp--;
				uCount = 0;
			}
			short sPCM = *(short *)(&pBuf[i]);
			sPCM = (short)(lAmp*((long)sPCM) / 100);
			*(short *)(&pBuf[i]) = sPCM;
		}
		return (int)i;
	}
private:
	unsigned int	m_uSampleBytes;
	unsigned int	m_uMaxSize;
	bool			m_bInFade;
	int				m_nDelta;
	unsigned int	m_uAmpPos;
	CRITICAL_SECTION m_csProct;
	unsigned int	m_uFilledInd;
	unsigned char	*m_pBufferArr[3];
};


int main()
{
	FILE *pfSrc = NULL;
	FILE *pfDst = NULL;

	fopen_s(&pfSrc, "d:/input.pcm", "rb");
	if (!pfSrc)
		return -1;

	fopen_s(&pfDst, "d:/output.pcm", "wb");
	if (!pfDst)
		return -1;

	static const unsigned int BUF_SIZE = 16 * 1024;
	unsigned char *pBuf = new unsigned char[BUF_SIZE];

	AudioSmooth *pProc = new AudioSmooth(16, BUF_SIZE);
	if (!pProc)
		return -1;

	int nSize = 0;
	int nCount = 0;
	bool bPositive = false;
	pProc->SetFade(2);
	while (nSize = fread(pBuf, 1, BUF_SIZE, pfSrc))
	{
		if (++nCount > 50)
		{
			if (bPositive)
				pProc->SetFade(2);
			else
				pProc->SetFade(-2);
			bPositive = !bPositive;
			nCount = 0;
		}
		int nProcSize = pProc->SmoothInput(pBuf, nSize);
		if (nProcSize > 0)
		{
			fwrite(pBuf, 1, nProcSize, pfDst);
		}
	}

	fclose(pfDst);
	fclose(pfSrc);
	if (pBuf)
	{
		delete[]pBuf;
	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
