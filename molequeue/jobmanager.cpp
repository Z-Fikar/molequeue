/******************************************************************************

  This source file is part of the MoleQueue project.

  Copyright 2012 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "jobmanager.h"

#include "job.h"
#include "jobdata.h"

#include <QtCore/QSettings>

namespace MoleQueue
{

JobManager::JobManager(QObject *parentObject) :
  QObject(parentObject)
{
  qRegisterMetaType<Job>("MoleQueue::Job");
}

JobManager::~JobManager()
{
  m_moleQueueMap.clear();
  qDeleteAll(m_jobs);
  m_jobs.clear();
}

void JobManager::readSettings(QSettings &settings)
{
  int numJobs = settings.beginReadArray("Jobs");
  for (int i = 0; i < numJobs; ++i) {
    settings.setArrayIndex(i);
    QVariantHash hash = settings.value("hash").toHash();
    JobData *jobdata = new JobData(this);
    jobdata->setFromHash(hash);
    m_jobs.append(jobdata);
    insertJobData(jobdata);
  }
  settings.endArray();
}

void JobManager::writeSettings(QSettings &settings) const
{
  settings.beginWriteArray("Jobs", m_jobs.size());
  for (int i = 0; i < m_jobs.size(); ++i) {
    settings.setArrayIndex(i);
    settings.setValue("hash", m_jobs.at(i)->hash());
  }
  settings.endArray(); // Jobs
}

Job JobManager::newJob()
{
  JobData *jobdata = new JobData(this);

  m_jobs.append(jobdata);
  emit jobAboutToBeAdded(Job(jobdata));

  insertJobData(jobdata);
  return Job(jobdata);
}

Job JobManager::newJob(const QVariantHash &jobState)
{
  JobData *jobdata = new JobData(this);
  jobdata->setFromHash(jobState);
  jobdata->setMoleQueueId(InvalidId);

  m_jobs.append(jobdata);
  emit jobAboutToBeAdded(Job(jobdata));

  insertJobData(jobdata);
  return Job(jobdata);
}

void JobManager::removeJob(JobData *jobdata)
{
  if (!jobdata || !m_jobs.contains(jobdata))
    return;

  emit jobAboutToBeRemoved(Job(jobdata));

  IdType moleQueueId = jobdata->moleQueueId();

  m_jobs.removeOne(jobdata);
  m_moleQueueMap.remove(moleQueueId);

  delete jobdata;

  emit jobRemoved(moleQueueId);
}

void JobManager::removeJob(IdType moleQueueId)
{
  JobData *jobdata = lookupJobDataByMoleQueueId(moleQueueId);

  if (jobdata)
    removeJob(jobdata);
}

void JobManager::removeJob(const Job &job)
{
  if (job.isValid())
    removeJob(job.jobData());
}

void JobManager::removeJobs(const QList<Job> &jobsToRemove)
{
  foreach (const Job &job, jobsToRemove)
    removeJob(job);
}

void JobManager::removeJobs(const QList<IdType> &moleQueueIds)
{
  foreach(IdType moleQueueId, moleQueueIds)
    removeJob(moleQueueId);
}

Job JobManager::lookupJobByMoleQueueId(IdType moleQueueId) const
{
  return Job(lookupJobDataByMoleQueueId(moleQueueId));
}

QList<Job> JobManager::jobsWithJobState(JobState state)
{
  QList<Job> result;

  foreach (JobData *jobdata, m_jobs) {
    if (jobdata->jobState() == state)
      result << Job(jobdata);
  }

  return result;
}

Job JobManager::jobAt(int i) const
{
  if (Q_LIKELY(i >= 0 && i < m_jobs.size()))
    return Job(m_jobs.at(i));
  return Job();
}

int JobManager::indexOf(const Job &job) const
{
  JobData *jobdata = job.jobData();
  if (jobdata)
    return m_jobs.indexOf(jobdata);

  return -1;
}

void JobManager::moleQueueIdChanged(const Job &job)
{
  JobData *jobdata = job.jobData();
  if (!m_jobs.contains(jobdata))
    return;

  if (lookupJobDataByMoleQueueId(jobdata->moleQueueId()) != jobdata) {
    IdType oldMoleQueueId = m_moleQueueMap.key(jobdata, InvalidId);
    if (oldMoleQueueId != InvalidId)
      m_moleQueueMap.remove(oldMoleQueueId);
    m_moleQueueMap.insert(jobdata->moleQueueId(), jobdata);
  }
}

void JobManager::setJobState(IdType moleQueueId, JobState newState)
{
  JobData *jobdata = lookupJobDataByMoleQueueId(moleQueueId);
  if (!jobdata)
    return;

  const JobState oldState = jobdata->jobState();

  if (oldState == newState)
    return;

  jobdata->setJobState(newState);

  emit jobStateChanged(jobdata, oldState, newState);
}

void JobManager::setJobQueueId(IdType moleQueueId, IdType queueId)
{
  JobData *jobdata = lookupJobDataByMoleQueueId(moleQueueId);
  if (!jobdata)
    return;

  if (jobdata->queueId() == queueId)
    return;

  jobdata->setQueueId(queueId);

  emit jobQueueIdChanged(jobdata);
}

void JobManager::insertJobData(JobData *jobdata)
{
  if (jobdata->moleQueueId() != MoleQueue::InvalidId)
    m_moleQueueMap.insert(jobdata->moleQueueId(), jobdata);

  emit jobAdded(Job(jobdata));
}

} // end namespace MoleQueue